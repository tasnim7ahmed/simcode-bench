#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 25.0;

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Configure Wi-Fi in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: Grid layout, random movement within rectangle
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(30.0),
                                  "DeltaY", DoubleValue(30.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()),
                              "MinX", DoubleValue(0.0),
                              "MinY", DoubleValue(0.0),
                              "MaxX", DoubleValue(60.0),
                              "MaxY", DoubleValue(60.0));
    mobility.Install(nodes);

    // Internet stack and IPv4 addressing
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP echo servers and clients in ring topology
    uint16_t portBase = 9000;
    ApplicationContainer servers;
    ApplicationContainer clients;

    for (uint32_t i = 0; i < 4; ++i)
    {
        // Echo server on each node
        UdpEchoServerHelper echoServer(portBase + i);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));
        servers.Add(serverApp);
    }
    for (uint32_t i = 0; i < 4; ++i)
    {
        uint32_t next = (i + 1) % 4;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(next), portBase + next);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
        clients.Add(clientApp);
    }

    // Enable FlowMonitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Enable animation output
    AnimationInterface anim("adhoc_ring_anim.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Collect flow and performance stats
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    double totalDelay = 0;
    double totalThroughput = 0;
    uint32_t flowCount = 0;

    std::cout << "\nFlow Statistics:\n";
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        uint32_t tx = flow.second.txPackets;
        uint32_t rx = flow.second.rxPackets;
        double throughput = rx * 1024.0 * 8 / (simTime - 2.0) / 1000.0; // kbps
        double avgDelay = rx ? (flow.second.delaySum.GetSeconds() / rx) : 0.0;
        double deliveryRatio = tx ? (double)rx / tx : 0.0;
        std::cout << "Flow " << flow.first
                  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << tx << "\n";
        std::cout << "  Rx Packets: " << rx << "\n";
        std::cout << "  Delivery Ratio: " << deliveryRatio * 100 << " %\n";
        std::cout << "  Average Delay: " << avgDelay * 1000 << " ms\n";
        std::cout << "  Throughput: " << throughput << " kbps\n";
        totalTxPackets += tx;
        totalRxPackets += rx;
        totalDelay += rx * avgDelay;
        totalThroughput += throughput;
        flowCount++;
    }

    if (flowCount > 0)
    {
        double packetDeliveryRatio = totalTxPackets ? (double)totalRxPackets / totalTxPackets : 0.0;
        double avgEndToEndDelay = totalRxPackets ? (totalDelay / totalRxPackets) : 0.0;
        double avgThroughput = (totalThroughput / flowCount);
        std::cout << "\nAggregate Performance Metrics:\n";
        std::cout << "  Packet Delivery Ratio: " << packetDeliveryRatio * 100 << " %\n";
        std::cout << "  Average End-to-End Delay: " << avgEndToEndDelay * 1000 << " ms\n";
        std::cout << "  Average Throughput: " << avgThroughput << " kbps\n";
    }

    Simulator::Destroy();
    return 0;
}