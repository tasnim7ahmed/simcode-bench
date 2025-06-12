#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(50.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(50.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port1 = 9, port2 = 10;

    UdpEchoServerHelper echoServer1(port1);
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(30.0));

    UdpEchoServerHelper echoServer2(port2);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(30.0));

    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), port2);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(30.0));

    UdpEchoClientHelper echoClient2(interfaces.GetAddress(0), port1);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(30.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("ad-hoc-2node.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 90.0, 50.0);

    Simulator::Stop(Seconds(32.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalTxPackets = 0, totalRxPackets = 0, totalDelay = 0;
    double totalThroughput = 0;
    uint32_t flowCount = 0;

    std::cout << "\nFlow Statistics:\n";
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        if (flow.second.rxPackets > 0)
        {
            double throughput = ((flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds())) / 1e3; // kbps
            std::cout << "  Throughput: " << throughput << " kbps\n";
            std::cout << "  Mean delay: " << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) << " s\n";
            totalTxPackets += flow.second.txPackets;
            totalRxPackets += flow.second.rxPackets;
            totalDelay += flow.second.delaySum.GetSeconds();
            totalThroughput += throughput;
            flowCount++;
        }
    }
    if (totalTxPackets > 0)
    {
        double pdr = (totalRxPackets / totalTxPackets) * 100.0;
        double meanDelay = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) : 0;
        double avgThroughput = (flowCount > 0) ? (totalThroughput / flowCount) : 0;
        std::cout << "\nAggregate Metrics:\n";
        std::cout << "  Packet Delivery Ratio: " << pdr << " %\n";
        std::cout << "  Mean End-to-End Delay: " << meanDelay << " s\n";
        std::cout << "  Average Throughput: " << avgThroughput << " kbps\n";
    }

    Simulator::Destroy();
    return 0;
}