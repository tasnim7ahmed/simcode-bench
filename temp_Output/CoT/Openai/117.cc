#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Configuration values
    double txPowerDbm = 50.0;
    uint32_t maxPackets = 50;
    double interval = 0.5; // seconds
    uint16_t echoPort = 9;
    double simStopTime = maxPackets * interval + 4.0; // observe all traffic

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure WiFi ad hoc
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility model: random between (0,0) - (100,100)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo apps on both nodes (bidirectional)
    // Node 0 runs server, Node 1 runs client to Node 0
    UdpEchoServerHelper echoServer0(echoPort);
    ApplicationContainer serverApp0 = echoServer0.Install(nodes.Get(0));
    serverApp0.Start(Seconds(1.0));
    serverApp0.Stop(Seconds(simStopTime));

    UdpEchoClientHelper echoClient0(interfaces.GetAddress(0), echoPort);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient0.Install(nodes.Get(1));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(simStopTime-1.0));

    // Node 1 runs server, Node 0 runs client to Node 1
    UdpEchoServerHelper echoServer1(echoPort);
    ApplicationContainer serverApp1 = echoServer1.Install(nodes.Get(1));
    serverApp1.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(simStopTime));

    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), echoPort);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp0 = echoClient1.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(simStopTime-1.0));

    // Flow monitor for statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim animation
    AnimationInterface anim("adhoc-echo-animation.xml");
    anim.SetMaxPktsPerTraceFile(500000);

    Simulator::Stop(Seconds(simStopTime));
    Simulator::Run();

    // Process flow monitor statistics
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    double totalDelay = 0;
    uint64_t totalRxBytes = 0;
    double firstTx = -1, lastRx = -1;

    for (auto const &flow : stats)
    {
        auto t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        double pd = (flow.second.txPackets > 0) ? (double)flow.second.rxPackets / flow.second.txPackets : 0;
        std::cout << "  Packet Delivery Ratio: " << pd << "\n";
        if (flow.second.rxPackets > 0) {
            double avgDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            std::cout << "  Average End-to-End Delay: " << avgDelay << " s\n";
        }
        double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1000.0; // kbps
        std::cout << "  Throughput: " << throughput << " kbps\n";

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalRxBytes += flow.second.rxBytes;
        if (firstTx < 0 || flow.second.timeFirstTxPacket.GetSeconds() < firstTx)
            firstTx = flow.second.timeFirstTxPacket.GetSeconds();
        if (lastRx < 0 || flow.second.timeLastRxPacket.GetSeconds() > lastRx)
            lastRx = flow.second.timeLastRxPacket.GetSeconds();
    }

    std::cout << "\n=== Aggregate Metrics ===\n";
    double overallPdr = totalTxPackets > 0 ? (double)totalRxPackets / totalTxPackets : 0;
    double overallDelay = totalRxPackets > 0 ? totalDelay / totalRxPackets : 0;
    double overallThroughput = (lastRx > firstTx && totalRxBytes > 0) ? (totalRxBytes * 8.0) / (lastRx - firstTx) / 1000.0 : 0; // kbps

    std::cout << "Aggregate Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Aggregate Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Overall Packet Delivery Ratio: " << overallPdr << "\n";
    std::cout << "Overall Average End-to-End Delay: " << overallDelay << " s\n";
    std::cout << "Overall Throughput: " << overallThroughput << " kbps\n";

    Simulator::Destroy();
    return 0;
}