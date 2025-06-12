#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWirelessRing");

int main(int argc, char *argv[])
{
    // Enable logging if you want:
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t nNodes = 4;
    double simDuration = 25.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Configure wifi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure wifi in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility: Grid with random movement within area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(50.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(mobility.GetPositionAllocator()),
                             "MinX", DoubleValue(0.0),
                             "MinY", DoubleValue(0.0),
                             "MaxX", DoubleValue(100.0),
                             "MaxY", DoubleValue(100.0));

    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install echo servers on all nodes
    uint16_t basePort = 9000;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nNodes; ++i) {
        UdpEchoServerHelper echoServer(basePort + i);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simDuration));
        serverApps.Add(serverApp);
    }

    // Each client sends to next node in ring
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nNodes; ++i) {
        uint32_t next = (i + 1) % nNodes;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(next), basePort + next);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simDuration - 1));
        clientApps.Add(clientApp);
    }

    // Install Flow Monitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Animation
    AnimationInterface anim("adhoc-ring.xml");

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    // Statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelaySum = 0;
    double totalThroughput = 0;
    double totalRxBytes = 0;

    std::cout << "\nFlow statistics (per flow):" << std::endl;
    for (const auto& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << "  "
                  << t.sourceAddress << " --> " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        if (flow.second.rxPackets > 0) {
            double delay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            std::cout << "  Mean delay: " << delay << " s" << std::endl;
        } else {
            std::cout << "  Mean delay: N/A" << std::endl;
        }
        double throughput = (flow.second.rxBytes * 8.0) / (simDuration - 2.0) / 1000.0; // Kbps
        std::cout << "  Throughput: " << throughput << " Kbps" << std::endl;
        std::cout << "  Packet Delivery Ratio: "
                  << ((flow.second.txPackets > 0) ? 100.0 * double(flow.second.rxPackets) / double(flow.second.txPackets) : 0.0)
                  << " %" << std::endl;

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelaySum += flow.second.delaySum.GetSeconds();
        totalThroughput += throughput;
        totalRxBytes += flow.second.rxBytes;
    }

    // Overall metrics
    double pdr = (totalTxPackets > 0) ? (100.0 * totalRxPackets / totalTxPackets) : 0.0;
    double avgDelay = (totalRxPackets > 0) ? (totalDelaySum / totalRxPackets) : 0.0;
    double avgThroughput = totalThroughput / stats.size();

    std::cout << "\n--- Overall performance metrics ---" << std::endl;
    std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "Average Throughput: " << avgThroughput << " Kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}