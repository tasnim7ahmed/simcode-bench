#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void
PrintFlowStats(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper &flowHelper)
{
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    std::map<FlowId, FlowMonitor::FlowStats> allStats = stats;
    Ipv4FlowClassifier::FiveTuple t;
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    for (auto iter = allStats.begin(); iter != allStats.end(); ++iter)
    {
        t = classifier->FindFlow(iter->first);
        std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds())/1024;
        std::cout << "  Throughput: " << throughput << " Kbps\n";
        std::cout << "  DelaySum: " << iter->second.delaySum.GetSeconds() << " s\n";
        std::cout << "  JitterSum: " << iter->second.jitterSum.GetSeconds() << " s\n";
        std::cout << "------------------------------------------\n";
    }
}

void
RunSimulation(bool enableRtsCts)
{
    std::cout << "=============================" << std::endl;
    if (enableRtsCts)
    {
        std::cout << "RUNNING WITH RTS/CTS ENABLED (Threshold = 100 bytes)" << std::endl;
    }
    else
    {
        std::cout << "RUNNING WITH RTS/CTS DISABLED" << std::endl;
    }

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<MatrixPropagationLossModel> matrix = CreateObject<MatrixPropagationLossModel>();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.SetPropagationLossModel(matrix);

    // Set 50 dB loss between adjacent nodes, and very high loss between non-adjacent (to simulate hidden).
    matrix->SetDefaultLossDb(200); // essentially prevents comms unless overridden
    matrix->SetLoss(nodes.Get(0), nodes.Get(1), 50);
    matrix->SetLoss(nodes.Get(1), nodes.Get(0), 50);
    matrix->SetLoss(nodes.Get(1), nodes.Get(2), 50);
    matrix->SetLoss(nodes.Get(2), nodes.Get(1), 50);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Set RTS/CTS threshold
    if (enableRtsCts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2200)); // disables
    }

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // Node 0
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));  // Node 1
    positionAlloc->Add(Vector(200.0, 0.0, 0.0));  // Node 2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port1 = 49152;
    uint16_t port2 = 49153;

    // CBR application 1: node 0 -> node 1
    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port1));
    onoff1.SetAttribute("DataRate", StringValue("2Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(512));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(1.000)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer app1 = onoff1.Install(nodes.Get(0));

    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApp1 = sink1.Install(nodes.Get(1));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(10.0));

    // CBR application 2: node 2 -> node 1 (slightly staggered)
    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port2));
    onoff2.SetAttribute("DataRate", StringValue("2Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(512));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(1.1)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));

    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp2 = sink2.Install(nodes.Get(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    PrintFlowStats(flowMonitor, flowHelper);

    Simulator::Destroy();
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2200));
}

int
main(int argc, char *argv[])
{
    // No RTS/CTS
    RunSimulation(false);

    // With RTS/CTS threshold 100
    RunSimulation(true);

    return 0;
}