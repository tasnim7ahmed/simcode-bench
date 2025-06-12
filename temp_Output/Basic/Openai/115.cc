#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Set up the Wi-Fi devices using 802.11
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // Set up mobility with RandomWalk2dMobilityModel within bounds
    MobilityHelper mobility;
    mobility.SetMobilityModel (
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue (20.0),
        "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]")
    );
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (10.0),
                                  "MinY", DoubleValue (10.0),
                                  "DeltaX", DoubleValue (80.0),
                                  "DeltaY", DoubleValue (0.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.Install (nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Install UDP echo server on node 1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer (port);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (15.0));

    // Install UDP echo client on node 0
    UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (64));

    ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (15.0));

    // Enable Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

    // Set up NetAnim output
    AnimationInterface anim ("adhoc-two-nodes.xml");
    anim.SetConstantPosition (nodes.Get (0), 25.0, 50.0);
    anim.SetConstantPosition (nodes.Get (1), 75.0, 50.0);

    Simulator::Stop (Seconds (16.0));
    Simulator::Run ();

    // FlowMonitor Statistics
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());

    double throughput = 0;
    double delaySum = 0;
    uint32_t rxPackets = 0;
    uint32_t txPackets = 0;
    uint32_t totalRxBytes = 0;

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (auto it = stats.begin (); it != stats.end (); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);

        // Only consider flows from client to server
        if ((t.sourceAddress == interfaces.GetAddress (0) && t.destinationAddress == interfaces.GetAddress (1)) ||
            (t.sourceAddress == interfaces.GetAddress (1) && t.destinationAddress == interfaces.GetAddress (0)))
        {
            std::cout << "Flow ID: " << it->first << std::endl;
            std::cout << "  Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
            std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
            std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
            std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
            std::cout << "  Throughput: " << (it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ()))/1000  << " kbps" << std::endl;
            if (it->second.rxPackets > 0)
            {
                std::cout << "  Mean delay: " << (it->second.delaySum.GetSeconds () / it->second.rxPackets) << " s" << std::endl;
            }
            else
            {
                std::cout << "  Mean delay: 0 s" << std::endl;
            }
            throughput += (it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ()));
            delaySum += it->second.delaySum.GetSeconds ();
            rxPackets += it->second.rxPackets;
            txPackets += it->second.txPackets;
            totalRxBytes += it->second.rxBytes;
        }
    }
    double pdr = txPackets > 0 ? ((double) rxPackets / txPackets) * 100.0 : 0.0;
    double avgDelay = rxPackets > 0 ? delaySum / rxPackets : 0.0;
    double netThroughput = (Simulator::Now ().GetSeconds () > 0) ? (totalRxBytes * 8.0 / Simulator::Now ().GetSeconds () / 1000.0) : 0.0;

    std::cout << "\n============== Overall Statistics ==============" << std::endl;
    std::cout << "Total Packets Sent:     " << txPackets << std::endl;
    std::cout << "Total Packets Received: " << rxPackets << std::endl;
    std::cout << "Packet Delivery Ratio:  " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "Total Throughput:       " << netThroughput << " kbps" << std::endl;
    std::cout << "Animation output written to adhoc-two-nodes.xml" << std::endl;

    Simulator::Destroy ();
    return 0;
}