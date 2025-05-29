#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiBidirectionalUdpEcho");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 20.0; // seconds
    uint32_t packetSize = 1024;
    uint32_t nPackets = 10;
    double interval = 1.0; // seconds
    uint32_t portA = 9;
    uint32_t portB = 10;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure WiFi in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: Random waypoint mobility within bounds
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator",
                             PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"))));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(10.0),
                                 "MinY", DoubleValue(10.0),
                                 "DeltaX", DoubleValue(30.0),
                                 "DeltaY", DoubleValue(30.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    // Internet stack and IP addressing
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer ifs = ipv4.Assign(devices);

    // Install UDP Echo server and client on both nodes for bidirectional traffic

    // Node 0 as server (portA), node 1 as client
    UdpEchoServerHelper echoServerA(portA);
    ApplicationContainer serverAppsA = echoServerA.Install(nodes.Get(0));
    serverAppsA.Start(Seconds(1.0));
    serverAppsA.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClientA(ifs.GetAddress(0), portA);
    echoClientA.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClientA.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClientA.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientAppsA = echoClientA.Install(nodes.Get(1));
    clientAppsA.Start(Seconds(2.0));
    clientAppsA.Stop(Seconds(simTime));

    // Node 1 as server (portB), node 0 as client
    UdpEchoServerHelper echoServerB(portB);
    ApplicationContainer serverAppsB = echoServerB.Install(nodes.Get(1));
    serverAppsB.Start(Seconds(1.0));
    serverAppsB.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClientB(ifs.GetAddress(1), portB);
    echoClientB.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClientB.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClientB.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientAppsB = echoClientB.Install(nodes.Get(0));
    clientAppsB.Start(Seconds(2.5));
    clientAppsB.Stop(Seconds(simTime));

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation
    AnimationInterface anim("adhoc-wifi-bidirectional.xml");
    anim.UpdateNodeDescription(0, "Node0");
    anim.UpdateNodeDescription(1, "Node1");
    anim.UpdateNodeColor(0, 255, 0, 0);
    anim.UpdateNodeColor(1, 0, 0, 255);

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    double sumThrougput = 0.0;
    uint32_t totalRxPackets = 0;
    uint32_t totalTxPackets = 0;
    uint32_t totalLostPackets = 0;
    double totalDelaySum = 0.0;
    uint32_t delaySamples = 0;

    std::cout << "\nFlow statistics:\n";
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        if (flow.second.rxPackets > 0)
        {
            // Calculate throughput
            double duration = (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds());
            double throughput = duration > 0 ? (flow.second.rxBytes * 8.0 / duration / 1024) : 0; // kbps
            std::cout << "  Throughput: " << throughput << " kbps\n";
            std::cout << "  Mean delay: "
                      << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) * 1000 << " ms\n";
            sumThrougput += throughput;
            totalRxPackets += flow.second.rxPackets;
            totalTxPackets += flow.second.txPackets;
            totalLostPackets += flow.second.lostPackets;
            totalDelaySum += flow.second.delaySum.GetSeconds();
            delaySamples += flow.second.rxPackets;
        }
        if (flow.second.packetsDropped.size() > 0)
        {
            uint32_t d = 0;
            for (auto it = flow.second.packetsDropped.begin(); it != flow.second.packetsDropped.end(); ++it)
            {
                d += it->second;
            }
            std::cout << "  Packets Dropped by reason: " << d << "\n";
        }
        std::cout << "--------------------------------------------\n";
    }

    // Compute and display aggregate metrics
    std::cout << "Aggregate statistics:\n";
    double pdr = totalTxPackets > 0 ? (double)totalRxPackets / totalTxPackets * 100.0 : 0;
    double meanDelayMs = delaySamples > 0 ? (totalDelaySum / delaySamples) * 1000 : 0;
    std::cout << "  Total Transmitted Packets: " << totalTxPackets << "\n";
    std::cout << "  Total Received Packets:    " << totalRxPackets << "\n";
    std::cout << "  Packet Delivery Ratio:     " << pdr << " %\n";
    std::cout << "  Total Lost Packets:        " << totalLostPackets << "\n";
    std::cout << "  Average End-to-End Delay:  " << meanDelayMs << " ms\n";
    std::cout << "  Sum Throughput:            " << sumThrougput << " kbps\n";

    Simulator::Destroy();
    return 0;
}