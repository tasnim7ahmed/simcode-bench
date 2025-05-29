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
    // Simulation parameters
    uint32_t numNodes = 4;
    double simTime = 25.0;
    double areaWidth = 100.0;
    double areaHeight = 100.0;
    double gridStep = 40.0;
    uint32_t maxPackets = 20;
    double interval = 0.5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wi-Fi AdHoc configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
    wifiPhy.Set("RxGain", DoubleValue(0));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: Grid position + random movement
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(gridStep),
                                 "DeltaY", DoubleValue(gridStep),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, areaWidth, 0.0, areaHeight)),
                             "Distance", DoubleValue(20.0),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Time", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(nodes);

    // Internet stack and IPv4
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo servers on all nodes
    uint16_t echoPort = 9;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        UdpEchoServerHelper echoServer(echoPort);
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo clients for ring topology
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t dstIdx = (i + 1) % numNodes;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(dstIdx), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1.0));

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // NetAnim
    AnimationInterface anim("adhoc_ring_anim.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0, totalRxPackets = 0;
    double totalDelay = 0.0, totalRxBytes = 0.0, throughput = 0.0;

    std::cout << "Flow statistics:\n";
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "FlowID: " << flow.first
                  << " Src: " << t.sourceAddress
                  << " Dst: " << t.destinationAddress << std::endl;

        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        std::cout << "  Delivered Ratio: " << (flow.second.rxPackets * 100.0 / (double)(flow.second.txPackets ? flow.second.txPackets : 1)) << " %" << std::endl;
        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Mean Delay: " << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) << " s" << std::endl;
        }
        else
        {
            std::cout << "  Mean Delay: N/A\n";
        }
        double flowThpt = flow.second.rxBytes * 8.0 / (simTime - 2.0) / 1000.0; // kbps
        std::cout << "  Throughput: " << flowThpt << " kbps" << std::endl;

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalDelay += flow.second.delaySum.GetSeconds();
        totalRxBytes += flow.second.rxBytes;
    }
    double avgPdr = (totalTxPackets ? totalRxPackets * 100.0 / totalTxPackets : 0.0);
    double avgDelay = (totalRxPackets ? totalDelay / totalRxPackets : 0.0);
    throughput = totalRxBytes * 8.0 / (simTime - 2.0) / 1000.0; // kbps

    std::cout << "\nAggregate Results:" << std::endl;
    std::cout << "  Total Tx Packets: " << totalTxPackets << std::endl;
    std::cout << "  Total Rx Packets: " << totalRxPackets << std::endl;
    std::cout << "  Packet Delivery Ratio: " << avgPdr << " %" << std::endl;
    std::cout << "  Average End-to-End Delay: " << avgDelay << " s" << std::endl;
    std::cout << "  Aggregate Throughput: " << throughput << " kbps" << std::endl;

    Simulator::Destroy();
    return 0;
}