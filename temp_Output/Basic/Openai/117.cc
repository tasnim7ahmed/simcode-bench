#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(50.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(50.0));
    wifiPhy.Set("TxGain", DoubleValue(0.0));
    wifiPhy.Set("RxGain", DoubleValue(0.0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-101.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-101.0));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                             "Distance", DoubleValue(20.0),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                             "Time", TimeValue(Seconds(2.0)));
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port1 = 9;
    uint16_t port2 = 10;

    UdpEchoServerHelper echoServer1(port1);
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(60.0));

    UdpEchoServerHelper echoServer2(port2);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), port2);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient2(interfaces.GetAddress(0), port1);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(60.0));

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    AnimationInterface anim("adhoc_2nodes.xml");
    anim.SetConstantPosition(nodes.Get(0), 20.0, 20.0);
    anim.SetConstantPosition(nodes.Get(1), 80.0, 80.0);

    Simulator::Stop(Seconds(62.0));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowsWithRx = 0;

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        if (flow.second.rxPackets > 0)
        {
            double delay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            std::cout << "  Mean Delay: " << delay << " s" << std::endl;
            double duration = (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds();
            double throughput = (flow.second.rxBytes * 8.0) / (duration * 1024.0);
            std::cout << "  Throughput: " << throughput << " Kbps" << std::endl;

            totalTxPackets += flow.second.txPackets;
            totalRxPackets += flow.second.rxPackets;
            totalDelay += delay;
            totalThroughput += throughput;
            ++flowsWithRx;
        }
        else
        {
            std::cout << "  Mean Delay: N/A (no received packets)" << std::endl;
            std::cout << "  Throughput: 0 Kbps" << std::endl;
            totalTxPackets += flow.second.txPackets;
        }
        std::cout << "-------------------------------------------------" << std::endl;
    }
    double pdr = (totalTxPackets > 0) ? ((double)totalRxPackets / totalTxPackets * 100.0) : 0.0;
    double meanDelay = (flowsWithRx > 0) ? (totalDelay / flowsWithRx) : 0.0;
    double meanThroughput = (flowsWithRx > 0) ? (totalThroughput / flowsWithRx) : 0.0;

    std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << meanDelay << " s" << std::endl;
    std::cout << "Average Throughput: " << meanThroughput << " Kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}