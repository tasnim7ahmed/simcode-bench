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

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint32_t numNodes = 4;
    double simTime = 22.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(40.0),
                                 "DeltaY", DoubleValue(40.0),
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

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t echoPort = 9;
    ApplicationContainer serverApps, clientApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        UdpEchoServerHelper echoServer(echoPort);
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i + 1) % numNodes;
        Ipv4Address dstAddr = interfaces.GetAddress(next);
        UdpEchoClientHelper echoClient(dstAddr, echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    AnimationInterface anim("adhoc-ring-animation.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    uint32_t totalRxPackets = 0;
    uint32_t totalTxPackets = 0;
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    std::cout << "Flow statistics:\n";
    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow ID: " << flow.first << " ";
        std::cout << t.sourceAddress << " -> " << t.destinationAddress << "\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Delivery Ratio: "
                  << (flow.second.txPackets > 0 ? double(flow.second.rxPackets)/flow.second.txPackets : 0) << "\n";
        std::cout << "  Throughput: "
                  << (flow.second.timeLastRxPacket.GetSeconds() > 0
                      ? flow.second.rxBytes * 8.0 /
                        (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1000
                      : 0)
                  << " Kbps\n";
        std::cout << "  Mean End-to-End Delay: "
                  << (flow.second.rxPackets > 0 ? flow.second.delaySum.GetSeconds() / flow.second.rxPackets : 0)
                  << " s\n";

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        if (flow.second.timeLastRxPacket.GetSeconds() > flow.second.timeFirstTxPacket.GetSeconds())
        {
            totalThroughput += flow.second.rxBytes * 8.0 /
                               (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1000;
        }
        ++flowCount;
    }
    std::cout << "\nOverall:\n";
    std::cout << "  Total Tx Packets: " << totalTxPackets << "\n";
    std::cout << "  Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "  Packet Delivery Ratio: "
              << (totalTxPackets > 0 ? double(totalRxPackets) / totalTxPackets : 0) << "\n";
    std::cout << "  Average End-to-End Delay: "
              << (totalRxPackets > 0 ? totalDelay / totalRxPackets : 0) << " s\n";
    std::cout << "  Average Throughput: "
              << (flowCount > 0 ? totalThroughput / flowCount : 0) << " Kbps\n";

    Simulator::Destroy();
    return 0;
}