#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/propagation-module.h"

using namespace ns3;

void RunSimulation(bool enableRts, uint32_t rtsThreshold)
{
    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    Ptr<MatrixPropagationLossModel> matrix = CreateObject<MatrixPropagationLossModel>();
    matrix->SetDefaultLoss(200); // Very high, nodes cannot talk if not set

    // Set loss between adjacent nodes to 50 dB
    matrix->SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);
    matrix->SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 50);

    matrix->SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 50);
    matrix->SetLoss(nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);

    // No direct link between node0 and node2
    matrix->SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 200);
    matrix->SetLoss(nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 200);

    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> chan = channel.Create();
    chan->SetPropagationLossModel(matrix);
    phy.SetChannel(chan);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Set RTS/CTS Threshold
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                       UintegerValue(rtsThreshold));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(0.0, 0.0, 0.0));   // Node 0
    pos->Add(Vector(5.0, 0.0, 0.0));   // Node 1
    pos->Add(Vector(10.0, 0.0, 0.0));  // Node 2
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port1 = 5001;
    uint16_t port2 = 5002;

    // Application 1: Node 0 -> Node 1
    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port1));
    onoff1.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(512));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApp1 = sink1.Install(nodes.Get(1));
    sinkApp1.Start(Seconds(0.5));
    sinkApp1.Stop(Seconds(10.01));

    // Application 2: Node 2 -> Node 1, start slightly later
    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port2));
    onoff2.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(512));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(1.01)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp2 = sink2.Install(nodes.Get(1));
    sinkApp2.Start(Seconds(0.5));
    sinkApp2.Stop(Seconds(10.01));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::string rtsStr = enableRts ? "ENABLED" : "DISABLED";
    std::cout << "===== Results with RTS/CTS " << rtsStr << " =====" << std::endl;
    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double time = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
        uint64_t rxBytes = flow.second.rxBytes;
        uint64_t txPackets = flow.second.txPackets;
        uint64_t rxPackets = flow.second.rxPackets;
        double throughput = time > 0 ? ((rxBytes * 8.0) / (time * 1000000.0)) : 0; // Mbps
        uint64_t lostPackets = txPackets - rxPackets;

        std::cout << "Flow ID: " << flow.first << " ("
                  << t.sourceAddress << ":" << t.sourcePort << " -> "
                  << t.destinationAddress << ":" << t.destinationPort << ")" << std::endl;
        std::cout << "  Tx Packets: " << txPackets << std::endl;
        std::cout << "  Rx Packets: " << rxPackets << std::endl;
        std::cout << "  Lost Packets: " << lostPackets << std::endl;
        std::cout << std::fixed << std::setprecision(6)
                  << "  Throughput: " << throughput << " Mbps" << std::endl;
    }
    std::cout << std::endl;

    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // First run: RTS/CTS disabled (threshold=2347, larger than any packet, disables RTS/CTS)
    RunSimulation(false, 2347);

    // Second run: RTS/CTS enabled for packets > 100 bytes
    RunSimulation(true, 100);

    return 0;
}