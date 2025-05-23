#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void RunSimulation(bool enableRtsCts) {
    std::cout << "\n=======================================================\n";
    std::cout << "Starting simulation with RTS/CTS " << (enableRtsCts ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "=======================================================\n";

    NodeContainer nodes;
    nodes.Create(3);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    nodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ConstantPositionMobilityModel>()->SetPosition(10.0, 0.0, 0.0);
    nodes.Get(2)->GetObject<ConstantPositionMobilityModel>()->SetPosition(20.0, 0.0, 0.0);

    Ptr<MatrixPropagationLossModel> matrixPropLoss = CreateObject<MatrixPropagationLossModel>();
    matrixPropLoss->Set(0, 1, 50);
    matrixPropLoss->Set(1, 0, 50);
    matrixPropLoss->Set(1, 2, 50);
    matrixPropLoss->Set(2, 1, 50);
    matrixPropLoss->Set(0, 2, 1000); // Very high loss for hidden terminal
    matrixPropLoss->Set(2, 0, 1000);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    Ptr<YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel>();
    wifiChannel->SetPropagationLossModel(matrixPropLoss);
    wifiPhy.SetChannel(wifiChannel);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", UintegerValue(100));
    } else {
        Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", UintegerValue(2200)); 
    }

    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    OnOffHelper onoff0("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
    onoff0.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff0.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff0.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff0.SetAttribute("PacketSize", UintegerValue(1024)); 
    ApplicationContainer apps0 = onoff0.Install(nodes.Get(0));
    apps0.Start(Seconds(1.0));
    apps0.Stop(Seconds(9.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024)); 
    ApplicationContainer apps2 = onoff2.Install(nodes.Get(2));
    apps2.Start(Seconds(1.1));
    apps2.Stop(Seconds(9.0));

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->CheckFor = FlowMonitor::CHECK_ALL_PACKETS;
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    uint32_t totalTxBytes = 0;
    uint32_t totalRxBytes = 0;
    uint32_t totalLostPackets = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.Begin(); i != stats.End(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000.0;
        std::cout << "  Throughput: " << throughput << " Mbps\n";
        std::cout << "  Packet Loss Rate: " << (double)i->second.lostPackets / i->second.txPackets * 100.0 << "%\n";

        totalTxBytes += i->second.txBytes;
        totalRxBytes += i->second.rxBytes;
        totalLostPackets += i->second.lostPackets;
    }

    std::cout << "\nAggregate Statistics:\n";
    std::cout << "  Total Tx Bytes: " << totalTxBytes << "\n";
    std::cout << "  Total Rx Bytes: " << totalRxBytes << "\n";
    std::cout << "  Total Lost Packets: " << totalLostPackets << "\n";
    std::cout << "  Total Throughput (approx): " << totalRxBytes * 8.0 / (Simulator::Now().GetSeconds() - 1.0) / 1000000.0 << " Mbps\n";

    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    RunSimulation(false); // Run once with RTS/CTS disabled
    RunSimulation(true);  // Run once with RTS/CTS enabled

    return 0;
}