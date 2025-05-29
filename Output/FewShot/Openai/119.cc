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
    uint32_t numNodes = 6;
    double simTime = 15.0;
    double areaWidth = 100.0;
    double areaHeight = 100.0;
    double gridSpace = 50.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(gridSpace),
                                 "DeltaY", DoubleValue(gridSpace),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", TimeValue(Seconds(1.0)),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=0.3|Max=1.0]"),
                             "Bounds", RectangleValue(Rectangle(0, areaWidth, 0, areaHeight)));
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t echoPort = 9;

    ApplicationContainer serverApps, clientApps;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        UdpEchoServerHelper echoServer(echoPort);
        ApplicationContainer server = echoServer.Install(nodes.Get(i));
        server.Start(Seconds(1.0));
        server.Stop(Seconds(simTime));
        serverApps.Add(server);
    }

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i + 1) % numNodes;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(next), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer client = echoClient.Install(nodes.Get(i));
        client.Start(Seconds(2.0));
        client.Stop(Seconds(simTime));
        clientApps.Add(client);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("adhoc-ring-animation.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    double totalDelay = 0.0;
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    std::cout << "Flow statistics:\n";
    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort != echoPort)
            continue;
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        double delivery = (flow.second.txPackets > 0) ? double(flow.second.rxPackets) / flow.second.txPackets : 0;
        std::cout << "  Packet Delivery Ratio: " << delivery * 100 << "%\n";
        double meanDelay = (flow.second.rxPackets > 0) ? double(flow.second.delaySum.GetMilliSeconds()) / flow.second.rxPackets : 0.0;
        std::cout << "  Average Delay: " << meanDelay << " ms\n";
        double throughput = (flow.second.rxBytes * 8.0) / (simTime - 2.0) / 1000.0; // kbps, client starts at 2.0
        std::cout << "  Throughput: " << throughput << " kbps\n";
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalThroughput += throughput;
        flowCount++;
    }

    std::cout << "\nOverall Performance Metrics:\n";
    double totalPdr = (totalTxPackets > 0) ? double(totalRxPackets) / totalTxPackets : 0;
    double avgDelay = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) * 1000.0 : 0.0;
    double avgThroughput = (flowCount > 0) ? totalThroughput / flowCount : 0.0;
    std::cout << "  Total Packet Delivery Ratio: " << totalPdr * 100 << "%\n";
    std::cout << "  Average End-to-End Delay: " << avgDelay << " ms\n";
    std::cout << "  Average Throughput: " << avgThroughput << " kbps\n";

    Simulator::Destroy();
    return 0;
}