#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    // WiFi PHY and MAC layer configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility configuration
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Applications (bidirectional)
    uint16_t port1 = 9;
    uint16_t port2 = 10;

    // Node 0: UDP Echo Server
    UdpEchoServerHelper echoServer1(port1);
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(15.0));

    // Node 1: UDP Echo Client
    UdpEchoClientHelper echoClient1(interfaces.GetAddress(0), port1);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(14.0));

    // Node 1: UDP Echo Server
    UdpEchoServerHelper echoServer2(port2);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(15.0));

    // Node 0: UDP Echo Client
    UdpEchoClientHelper echoClient2(interfaces.GetAddress(1), port2);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(0));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(14.5));

    // Install Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation
    AnimationInterface anim("adhoc-bidirectional.xml");

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();

    // Flow monitor analysis
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalTxPackets = 0.0;
    double totalRxPackets = 0.0;
    double totalDelay = 0.0;
    double totalRxBytes = 0.0;
    double simulationTime = 16.0 - 1.0; //time between first start & global stop

    std::cout << "Flow statistics:\n";
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "FlowID: " << flow.first << " "
                  << t.sourceAddress << " --> " << t.destinationAddress << "\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Throughput: "
                  << (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1000
                  << " kbps\n";
        std::cout << "  Mean Delay: "
                  << (flow.second.rxPackets > 0 ? flow.second.delaySum.GetSeconds() / flow.second.rxPackets : 0) << " s\n";
        std::cout << "  Packet Delivery Ratio: "
                  << (flow.second.txPackets > 0 ? flow.second.rxPackets * 100.0 / flow.second.txPackets : 0) << " %\n\n";

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalRxBytes += flow.second.rxBytes;
    }
    double pdr = (totalTxPackets > 0) ? totalRxPackets * 100.0 / totalTxPackets : 0;
    double meanDelay = (totalRxPackets > 0) ? totalDelay / totalRxPackets : 0;
    double throughput = (totalRxBytes * 8) / (simulationTime * 1000);

    std::cout << "*** Overall Performance Metrics ***\n";
    std::cout << "Total Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Aggregate Packet Delivery Ratio: " << pdr << " %\n";
    std::cout << "Aggregate Mean End-to-End Delay: " << meanDelay << " s\n";
    std::cout << "Aggregate Throughput: " << throughput << " kbps\n";

    Simulator::Destroy();
    return 0;
}