#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 30.0;

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Configure WiFi for Ad-Hoc
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypoint within a 100x100 area
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", StringValue(
                                 "ns3::RandomRectanglePositionAllocator["
                                 "X=ns3::UniformRandomVariable[Min=0.0|Max=100.0],"
                                 "Y=ns3::UniformRandomVariable[Min=0.0|Max=100.0]]"));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(10.0),
                                 "MinY", DoubleValue(10.0),
                                 "DeltaX", DoubleValue(30.0),
                                 "DeltaY", DoubleValue(30.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(devices);

    // Install UDP traffic: node 0 -> node 3 and node 1 -> node 2
    uint16_t udpPort1 = 5001;
    uint16_t udpPort2 = 5002;

    // On node 3: UDP server
    UdpServerHelper udpServer1(udpPort1);
    ApplicationContainer serverApp1 = udpServer1.Install(nodes.Get(3));
    serverApp1.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(simTime));

    // On node 2: UDP server
    UdpServerHelper udpServer2(udpPort2);
    ApplicationContainer serverApp2 = udpServer2.Install(nodes.Get(2));
    serverApp2.Start(Seconds(1.0));
    serverApp2.Stop(Seconds(simTime));

    // On node 0: UDP client to node 3
    UdpClientHelper udpClient1(ifaces.GetAddress(3), udpPort1);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient1.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp1 = udpClient1.Install(nodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(simTime));

    // On node 1: UDP client to node 2
    UdpClientHelper udpClient2(ifaces.GetAddress(2), udpPort2);
    udpClient2.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient2.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp2 = udpClient2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(simTime));

    // Enable NetAnim
    AnimationInterface anim("aodv-adhoc-netanim.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(nodes.Get(i), "N" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 0, 255 - 60 * i, 100 + 50 * i);
    }

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0, totalRxPackets = 0;
    double delaySum = 0.0;
    double throughputSum = 0.0;
    uint32_t flowCount = 0;

    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        delaySum += (flow.second.delaySum.GetSeconds());
        // Throughput in bit/s
        if (flow.second.timeLastRxPacket.GetSeconds() > 0 && flow.second.timeFirstTxPacket.GetSeconds() >= 0)
        {
            double flowDuration = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
            if (flowDuration > 0.0)
            {
                throughputSum += ((flow.second.rxBytes * 8.0) / flowDuration);
                flowCount++;
            }
        }
    }

    double pdr = (totalTxPackets > 0) ? ((double)totalRxPackets / totalTxPackets) * 100 : 0.0;
    double avgDelay = (totalRxPackets > 0) ? (delaySum / totalRxPackets) : 0.0;
    double avgThroughput = (flowCount > 0) ? (throughputSum / flowCount) / 1e3 : 0.0; // kbps

    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "Average Throughput: " << avgThroughput << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}