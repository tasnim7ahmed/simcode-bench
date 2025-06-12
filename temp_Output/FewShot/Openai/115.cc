#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable application and wifi logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("WifiMac", LOG_LEVEL_INFO);
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up WiFi in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set up mobility: RandomWalk2dMobilityModel in bounds
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "Distance", DoubleValue(5.0),
                             "Time", TimeValue(Seconds(1.0)));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=90.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=90.0]"));
    mobility.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UdpEchoServer on node 1 and client on node 0
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Create NetAnim animation file
    AnimationInterface anim("adhoc-2nodes.xml");
    anim.SetMobilityPollInterval(Seconds(1.0));

    // Run the simulation
    Simulator::Stop(Seconds(21.0));
    Simulator::Run();

    // Process FlowMonitor statistics
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelay = 0;
    double totalRxBytes = 0;
    double simulationTime = 18.0; // Client runs from 2s to 20s

    std::cout << "Flow Statistics\n";
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " Src Addr " << t.sourceAddress
                  << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        if (flow.second.rxPackets > 0)
        {
            double avgDelayMs = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) * 1000;
            std::cout << "  Average End-to-End Delay: " << avgDelayMs << " ms" << std::endl;
            totalDelay += flow.second.delaySum.GetSeconds();
        }
        else
        {
            std::cout << "  Average End-to-End Delay: N/A\n";
        }
        std::cout << "  Throughput: " 
                  << (flow.second.rxBytes * 8.0) / (simulationTime * 1000.0) // kbit/s
                  << " kbit/s" << std::endl;
        double pdf = flow.second.txPackets > 0 ? double(flow.second.rxPackets) / flow.second.txPackets * 100.0 : 0.0;
        std::cout << "  Packet Delivery Ratio: " << pdf << " %" << std::endl;
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalRxBytes += flow.second.rxBytes;
    }

    if (totalTxPackets > 0)
    {
        double overallPdr = (totalRxPackets / totalTxPackets) * 100.0;
        double overallAvgDelay = totalRxPackets > 0 ? (totalDelay / totalRxPackets) * 1000 : 0.0;
        double overallThroughput = (totalRxBytes * 8.0) / (simulationTime * 1000.0); // kbit/s

        std::cout << "\n===== Overall Statistics =====" << std::endl;
        std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
        std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
        std::cout << "Packet Delivery Ratio: " << overallPdr << " %" << std::endl;
        std::cout << "Average End-to-End Delay: " << overallAvgDelay << " ms" << std::endl;
        std::cout << "Throughput: " << overallThroughput << " kbit/s" << std::endl;
    }
    else
    {
        std::cout << "No packets transmitted.\n";
    }

    Simulator::Destroy();
    return 0;
}