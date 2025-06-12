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
    Time::SetResolution (Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set ("TxPowerStart", DoubleValue (50.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (50.0));
    wifiPhy.Set ("RxGain", DoubleValue (0));
    wifiPhy.Set ("TxGain", DoubleValue (0));
    wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-96.0));
    wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-99.0));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel (wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue ("Time"),
                              "Time", StringValue ("2s"),
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Bounds", RectangleValue (Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port1 = 9;
    uint16_t port2 = 10;

    // Echo server on node 0 (port 9)
    UdpEchoServerHelper echoServer1(port1);
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(30.0));

    // Echo server on node 1 (port 10)
    UdpEchoServerHelper echoServer2(port2);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(30.0));

    // Echo client on node 0, sending to node 1
    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), port2);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(30.0));

    // Echo client on node 1, sending to node 0
    UdpEchoClientHelper echoClient2(interfaces.GetAddress(0), port1);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(30.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("adhoc-2node.xml");

    Simulator::Stop(Seconds(32.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    double totalDelayMs = 0;
    double minStart = 1e9;
    double maxLast = 0;
    uint32_t totalRxBytes = 0;

    std::cout << "Flow statistics:\n";
    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " "
                  << t.sourceAddress << ":" << t.sourcePort << " -> "
                  << t.destinationAddress << ":" << t.destinationPort << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        std::cout << "  Throughput: "
                  << (flow.second.rxBytes * 8.0) / ((flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) * 1000.0)
                  << " kbps" << std::endl;

        if (flow.second.rxPackets > 0)
            std::cout << "  Mean delay: " << (flow.second.delaySum.GetMilliSeconds() / flow.second.rxPackets) << " ms" << std::endl;
        else
            std::cout << "  Mean delay: N/A" << std::endl;

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelayMs += flow.second.delaySum.GetMilliSeconds();
        totalRxBytes += flow.second.rxBytes;

        if (flow.second.timeFirstTxPacket.GetSeconds() < minStart)
            minStart = flow.second.timeFirstTxPacket.GetSeconds();
        if (flow.second.timeLastRxPacket.GetSeconds() > maxLast)
            maxLast = flow.second.timeLastRxPacket.GetSeconds();
    }

    double pdr = (totalTxPackets > 0) ? (100.0 * totalRxPackets / totalTxPackets) : 0.0;
    double avgDelay = (totalRxPackets > 0) ? (totalDelayMs / totalRxPackets) : 0.0;
    double duration = (maxLast - minStart);
    double throughput = (duration > 0) ? ((totalRxBytes * 8.0) / (duration * 1000.0)) : 0.0; // kbps

    std::cout << "\nAggregate Performance Metrics:\n";
    std::cout << "  Total sent packets: " << totalTxPackets << std::endl;
    std::cout << "  Total received packets: " << totalRxPackets << std::endl;
    std::cout << "  Packet Delivery Ratio: " << pdr << " %\n";
    std::cout << "  Average End-to-End Delay: " << avgDelay << " ms\n";
    std::cout << "  Aggregate Throughput: " << throughput << " kbps\n";

    Simulator::Destroy();
    return 0;
}