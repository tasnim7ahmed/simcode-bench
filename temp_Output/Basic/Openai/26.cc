#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/propagation-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <iomanip>

using namespace ns3;

void PrintResults(std::string label, FlowMonitorHelper &flowmon, Ptr<FlowMonitor> monitor)
{
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    Ipv4FlowClassifier::FlowIdToFlowClassMap classifierMap = flowmon.GetClassifier()->GetFlowIdToFlowClassMap();
    std::cout << "\n====== " << label << " ======" << std::endl;
    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = flowmon.GetClassifier()->FindFlow(flow.first);
        std::cout << "Flow " << flow.first
                  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        double throughput = flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1000.0;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Throughput: " << throughput << " Kbps\n";
        std::cout << "  Packets Lost Ratio: " << ((flow.second.txPackets > 0) ? (double)flow.second.lostPackets/flow.second.txPackets : 0) << "\n";
    }
    std::cout << "===========================\n";
}

void RunScenario(bool enableRtsCts)
{
    NodeContainer nodes;
    nodes.Create(3);

    // Mobility: Node 0 at 0m, Node 1 at 100m, Node 2 at 200m
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
    pos->Add(Vector(0.0, 0.0, 0.0));
    pos->Add(Vector(100.0, 0.0, 0.0));
    pos->Add(Vector(200.0, 0.0, 0.0));
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Wi-Fi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
    // Infinite loss (default) between all pairs
    // Set only adjacent nodes to 50dB, others (hidden) infinite
    lossModel->SetDefaultLossDb(1000); // effectively infinite
    lossModel->SetLoss (nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);
    lossModel->SetLoss (nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 50);
    lossModel->SetLoss (nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel(), 50);
    lossModel->SetLoss (nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);

    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel> ();
    channel->SetPropagationLossModel(lossModel);
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    phy.SetChannel(channel);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // RTS/CTS threshold set as per scenario
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(enableRtsCts ? 100 : 2347)); // 2347 disables

    WifiRemoteStationManager::FragmentationThreshold = 2346;

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port1 = 5000, port2 = 5001;

    // Application parameters
    double start0 = 1.0, stop0 = 10.0;
    double start2 = 1.2, stop2 = 10.0;
    uint32_t packetSize = 512;
    std::string dataRate = "1Mbps";

    //.Node 0 -> Node 1
    OnOffHelper onoff1 ("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port1)));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff1.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(start0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(stop0)));
    ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
    // Sink at Node 1 for flow 1
    PacketSinkHelper sink1 ("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port1)));
    ApplicationContainer sinkApp1 = sink1.Install(nodes.Get(1));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(stop0));

    // Node 2 -> Node 1
    OnOffHelper onoff2 ("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port2)));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff2.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(start2)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(stop2)));
    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
    // Sink at Node 1 for flow 2
    PacketSinkHelper sink2 ("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port2)));
    ApplicationContainer sinkApp2 = sink2.Install(nodes.Get(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(stop2));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.1));
    Simulator::Run();

    std::string label = enableRtsCts ? "With RTS/CTS" : "Without RTS/CTS";
    PrintResults(label, flowmon, monitor);

    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    std::cout << "======= Hidden Terminal Problem Simulation =======\n";
    RunScenario(false);
    RunScenario(true);
    std::cout << "=================================================\n";
    return 0;
}