#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/onoff-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

void PrintFlowMonitorStats(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper& flowHelper)
{
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i;
    Ipv4FlowClassifier::FiveTuple t;
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    for (i = stats.begin(); i != stats.end(); ++i)
    {
        t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " SrcAddr: " << t.sourceAddress
                  << " DstAddr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets:   " << i->second.txPackets << std::endl;
        std::cout << "  Rx Packets:   " << i->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
        double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1e6;
        std::cout << "  Throughput:   " << throughput << " Mbps" << std::endl << std::endl;
    }
}

void RunSimulation(bool rtsCtsEnabled)
{
    std::cout << std::endl;
    std::cout << "=============== Simulation ";
    if (rtsCtsEnabled)
        std::cout << "WITH";
    else
        std::cout << "WITHOUT";
    std::cout << " RTS/CTS ===============" << std::endl;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Set up Wi-Fi 802.11b in ad-hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    // Set up MatrixPropagationLossModel: 0-1 and 1-2 see each other, 0-2 are hidden terminals
    Ptr<MatrixPropagationLossModel> matrix = CreateObject<MatrixPropagationLossModel>();
    matrix->SetDefaultLossDb(200); // Anything not set is very high: effectively no signal
    matrix->SetLoss(0, 1, 50); // node0 <-> node1: 50 dB
    matrix->SetLoss(1, 0, 50);
    matrix->SetLoss(1, 2, 50); // node1 <-> node2: 50 dB
    matrix->SetLoss(2, 1, 50);
    // node0 <-> node2: leave at 200 dB (default)
    Ptr<YansWifiChannel> chan = CreateObject<YansWifiChannel>();
    chan->SetPropagationLossModel(matrix);
    phy.SetChannel(chan);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set RTS/CTS threshold
    if (rtsCtsEnabled)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100)); // bytes
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2200)); // bytes, disables RTS/CTS
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up static mobility: 0 - 1 - 2, positions: 0m, 100m, 200m
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // node 0
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));   // node 1
    positionAlloc->Add(Vector(200.0, 0.0, 0.0));   // node 2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Generate CBR stream: OnOffApplication
    uint16_t port1 = 5001;
    uint16_t port2 = 5002;

    // Node 0 -> Node 1
    OnOffHelper onoff0("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port1));
    onoff0.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff0.SetAttribute("PacketSize", UintegerValue(512));
    onoff0.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff0.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app0 = onoff0.Install(nodes.Get(0));
    app0.Start(Seconds(1.0));
    app0.Stop(Seconds(10.0));

    // Node 2 -> Node 1 (slightly staggered to avoid overlap at zero time)
    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port2));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(512));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
    app2.Start(Seconds(1.1));
    app2.Stop(Seconds(10.0));

    // Set up packet sinks on node 1 for both flows
    PacketSinkHelper sinkHelper1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp1 = sinkHelper1.Install(nodes.Get(1));
    ApplicationContainer sinkApp2 = sinkHelper2.Install(nodes.Get(1));
    sinkApp1.Start(Seconds(0.0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(11.0));
    sinkApp2.Stop(Seconds(11.0));

    // Install FlowMonitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // FlowMonitor stats
    PrintFlowMonitorStats(flowMonitor, flowHelper);

    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // Disable logging to keep output minimal

    // First run: RTS/CTS disabled
    RunSimulation(false);

    // Second run: RTS/CTS enabled for packets >100 bytes
    RunSimulation(true);

    return 0;
}