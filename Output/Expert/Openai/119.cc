#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    double simTime = 15.0;
    double packetInterval = 0.5;
    uint32_t maxPackets = 20;
    uint16_t echoPort = 9;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(40.0),
                                 "DeltaY", DoubleValue(40.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0,120,0,80)),
                             "Distance", DoubleValue(15.0),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=3.0]"));
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    std::vector<ApplicationContainer> echoServers(numNodes);
    std::vector<ApplicationContainer> echoClients(numNodes);

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        UdpEchoServerHelper echoServer(echoPort);
        echoServers[i] = echoServer.Install(nodes.Get(i));
        echoServers[i].Start(Seconds(0.2));
        echoServers[i].Stop(Seconds(simTime));

        uint32_t dest = (i + 1) % numNodes;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(dest), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        echoClients[i] = echoClient.Install(nodes.Get(i));
        echoClients[i].Start(Seconds(1.0));
        echoClients[i].Stop(Seconds(simTime - 1));
    }

    AnimationInterface anim("adhoc-grid-ring.xml");

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    std::cout << "Flow statistics:\n";
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        if (flow.second.rxPackets > 0)
        {
            double delay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            double throughput = flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024.0;
            std::cout << "  Avg Delay: " << delay << " s\n";
            std::cout << "  Throughput: " << throughput << " kbps\n";
            totalDelay += flow.second.delaySum.GetSeconds();
            totalThroughput += throughput;
        }
        else
        {
            std::cout << "  Avg Delay: N/A\n  Throughput: 0 kbps\n";
        }
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        ++flowCount;
    }

    double pdr = totalTxPackets ? 100.0 * totalRxPackets / totalTxPackets : 0.0;
    double avgDelay = totalRxPackets ? totalDelay / totalRxPackets : 0.0;
    double avgThroughput = flowCount ? totalThroughput / flowCount : 0.0;

    std::cout << "\nAggregate metrics:\n";
    std::cout << "  Packet Delivery Ratio: " << pdr << " %\n";
    std::cout << "  Average End-to-End Delay: " << avgDelay << " s\n";
    std::cout << "  Average Throughput: " << avgThroughput << " kbps\n";

    Simulator::Destroy();
    return 0;
}