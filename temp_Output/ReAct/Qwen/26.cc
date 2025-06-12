#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenTerminalRtsCts");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for packets larger than 100 bytes", enableRtsCts);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    auto channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MatrixPropagationLossModel lossModel;
    lossModel.SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), DbmToPower(-50));
    lossModel.SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), DbmToPower(-50));
    lossModel.SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), DbmToPower(-50));
    lossModel.SetLoss(nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), DbmToPower(-50));

    channel->AddPropagationLossModel(&lossModel);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff1.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
    app2.Start(Seconds(1.1));
    app2.Stop(Seconds(10.0));

    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RTSThreshold", UintegerValue(100));
    } else {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RTSThreshold", UintegerValue(3000));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "Simulation run with RTS/CTS " << (enableRtsCts ? "enabled" : "disabled") << ":" << std::endl;
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::cout << "Flow ID: " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}