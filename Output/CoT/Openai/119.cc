#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    uint32_t nNodes = 6;
    double simTime = 20.0;

    NodeContainer nodes;
    nodes.Create(nNodes);

    // WiFi configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility: grid layout with random movement in bounds
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(50.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomRectanglePositionAllocator",
                             "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=150.0]"),
                             "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    std::ostringstream bounds;
    bounds << "0.0|" << 150.0 << "|0.0|" << 100.0;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 150.0, 0.0, 100.0)),
                              "Distance", DoubleValue(10.0),
                              "Time", TimeValue(Seconds(2.0)));
    mobility.Install(nodes);

    // Install Internet and set IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP echo servers and clients in a ring
    uint16_t echoPort = 9;
    std::vector<ApplicationContainer> servers;
    for (uint32_t i = 0; i < nNodes; ++i) {
        UdpEchoServerHelper echoServer(echoPort);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(simTime));
        servers.push_back(serverApp);
    }
    // Ring: node i client to node (i+1)%n server
    for (uint32_t i = 0; i < nNodes; ++i) {
        uint32_t next = (i + 1) % nNodes;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(next), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
    }

    // Enable Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation output
    AnimationInterface anim("adhoc_ring_anim.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Gather and print flow statistics and metrics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    double totalDelaySum = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowsCounted = 0;

    std::cout << "Flow statistics:\n";
    for (auto const &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        if (flow.second.rxPackets > 0) {
            double meanDelay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            std::cout << "  Mean delay: " << meanDelay << " s\n";
        } else {
            std::cout << "  Mean delay: N/A\n";
        }
        double throughput = flow.second.rxBytes * 8.0 / (simTime - 2.0) / 1000.0; // kbps
        std::cout << "  Throughput: " << throughput << " kbps\n";

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelaySum += flow.second.delaySum.GetSeconds();
        totalThroughput += throughput;
        if (flow.second.rxPackets > 0) ++flowsCounted;
    }
    std::cout << "\n==== Aggregate Performance Metrics ====\n";
    double pdr = (totalTxPackets > 0) ? (100.0 * totalRxPackets / totalTxPackets) : 0.0;
    double meanE2EDelay = (totalRxPackets > 0) ? (totalDelaySum / totalRxPackets) : 0.0;
    double meanThroughput = (flowsCounted > 0) ? (totalThroughput / flowsCounted) : 0.0;
    std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %\n";
    std::cout << "Mean End-to-End Delay: " << meanE2EDelay << " s\n";
    std::cout << "Mean Throughput: " << meanThroughput << " kbps\n";

    Simulator::Destroy();
    return 0;
}