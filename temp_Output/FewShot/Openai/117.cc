#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    // Wi-Fi channel and PHY configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(50.0));
    phy.Set("TxPowerEnd", DoubleValue(50.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility configuration: random movement within bounds
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomRectanglePositionAllocator",
                             "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                             "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    // Internet stack and IP assignment
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Servers on both nodes
    uint16_t portA = 9001;
    uint16_t portB = 9002;

    // Server on node 0
    UdpEchoServerHelper serverA(portA);
    ApplicationContainer serverAppA = serverA.Install(nodes.Get(0));
    serverAppA.Start(Seconds(1.0));
    serverAppA.Stop(Seconds(30.0));

    // Server on node 1
    UdpEchoServerHelper serverB(portB);
    ApplicationContainer serverAppB = serverB.Install(nodes.Get(1));
    serverAppB.Start(Seconds(1.0));
    serverAppB.Stop(Seconds(30.0));

    // UDP Echo Client from node 0 -> node 1 (to portB)
    UdpEchoClientHelper clientA(interfaces.GetAddress(1), portB);
    clientA.SetAttribute("MaxPackets", UintegerValue(50));
    clientA.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    clientA.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppA = clientA.Install(nodes.Get(0));
    clientAppA.Start(Seconds(2.0));
    clientAppA.Stop(Seconds(30.0));

    // UDP Echo Client from node 1 -> node 0 (to portA)
    UdpEchoClientHelper clientB(interfaces.GetAddress(0), portA);
    clientB.SetAttribute("MaxPackets", UintegerValue(50));
    clientB.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    clientB.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppB = clientB.Install(nodes.Get(1));
    clientAppB.Start(Seconds(2.0));
    clientAppB.Stop(Seconds(30.0));

    // Flow Monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // NetAnim
    AnimationInterface anim("adhoc_bidirectional.xml");

    Simulator::Stop(Seconds(31.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalRxPackets = 0.0, totalTxPackets = 0.0, totalRxBytes = 0.0, totalTxBytes = 0.0;
    double totalThroughput = 0.0, totalDelay = 0.0, totalDeliveredFlows = 0.0;

    std::cout << "Flow statistics:\n";
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " Src: " << t.sourceAddress << " Dst: " << t.destinationAddress << "\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Tx Bytes: " << flow.second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << flow.second.rxBytes << "\n";
        double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024.0; // kbps
        std::cout << "  Throughput: " << throughput << " kbps\n";
        if (flow.second.rxPackets > 0)
        {
            double meanDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            std::cout << "  Mean delay: " << meanDelay << " s\n";
            totalDelay += flow.second.delaySum.GetSeconds();
            ++totalDeliveredFlows;
        }
        else
        {
            std::cout << "  Mean delay: N/A\n";
        }
        std::cout << "  Packet Delivery Ratio: "
                  << ((flow.second.txPackets > 0) ? (double(flow.second.rxPackets) / flow.second.txPackets) * 100 : 0)
                  << "%\n";

        totalRxPackets += flow.second.rxPackets;
        totalTxPackets += flow.second.txPackets;
        totalRxBytes += flow.second.rxBytes;
        totalTxBytes += flow.second.txBytes;
        totalThroughput += throughput;
    }

    // Overall metrics
    std::cout << "\nTotal Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Total Tx Bytes: " << totalTxBytes << "\n";
    std::cout << "Total Rx Bytes: " << totalRxBytes << "\n";
    std::cout << "Overall Packet Delivery Ratio: " << (totalTxPackets > 0 ? (totalRxPackets / totalTxPackets) * 100.0 : 0) << "%\n";
    std::cout << "Average Throughput: " << (totalDeliveredFlows > 0 ? totalThroughput / totalDeliveredFlows : 0) << " kbps\n";
    std::cout << "Average End-to-End Delay: " << (totalRxPackets > 0 ? totalDelay / totalRxPackets : 0) << " s\n";

    Simulator::Destroy();

    return 0;
}