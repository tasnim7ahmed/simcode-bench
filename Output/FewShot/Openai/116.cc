#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    //LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    //LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    double simulationTime = 20.0; // seconds

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure WiFi (802.11) in ad hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set random mobility model within bounds
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo servers (one on each node)
    uint16_t port1 = 1111;
    uint16_t port2 = 2222;
    UdpEchoServerHelper echoServer1(port1);
    UdpEchoServerHelper echoServer2(port2);

    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(0));
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(1));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(simulationTime));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(simulationTime));

    // UDP Echo clients (each node sends to the other's server)
    UdpEchoClientHelper echoClientTo2(interfaces.GetAddress(1), port2);
    echoClientTo2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientTo2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientTo2.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps1 = echoClientTo2.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClientTo1(interfaces.GetAddress(0), port1);
    echoClientTo1.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientTo1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientTo1.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps2 = echoClientTo1.Install(nodes.Get(1));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(simulationTime));

    // Install FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Set up animation
    AnimationInterface anim("adhoc-animation.xml");

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Collect flow monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalDelivered = 0;
    double totalLost = 0;
    double totalDelay = 0;
    double totalTx = 0;
    double totalRx = 0;
    double totalThroughput = 0;
    uint32_t flows = 0;

    std::cout << "Flow statistics:" << std::endl;
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Delivery Ratio: ";
        if (flow.second.txPackets > 0)
            std::cout << (double) flow.second.rxPackets / flow.second.txPackets * 100.0 << " %\n";
        else
            std::cout << "NA\n";
        std::cout << "  End-to-end delay (s): ";
        if (flow.second.rxPackets > 0)
            std::cout << flow.second.delaySum.GetSeconds() / flow.second.rxPackets << "\n";
        else
            std::cout << "NA\n";
        double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024;
        std::cout << "  Throughput: " << throughput << " kbps\n";
        flows++;
        totalDelivered += flow.second.rxPackets;
        totalLost += flow.second.lostPackets;
        if (flow.second.rxPackets > 0)
            totalDelay += flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
        totalTx += flow.second.txPackets;
        totalRx += flow.second.rxPackets;
        totalThroughput += throughput;
    }
    std::cout << "\nAggregated Results:\n";
    std::cout << "  Average Packet Delivery Ratio: ";
    if (totalTx > 0)
        std::cout << (totalRx / totalTx) * 100.0 << " %\n";
    else
        std::cout << "NA\n";
    std::cout << "  Average End-to-End Delay: ";
    if (flows > 0)
        std::cout << totalDelay / flows << " s\n";
    else
        std::cout << "NA\n";
    std::cout << "  Average Throughput: ";
    if (flows > 0)
        std::cout << totalThroughput / flows << " kbps\n";
    else
        std::cout << "NA\n";

    Simulator::Destroy();
    return 0;
}