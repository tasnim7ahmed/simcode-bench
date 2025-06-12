#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/on-off-application.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/mpdu-aggregation-parameters.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpAggregation");

int main(int argc, char* argv[]) {
    bool enablePcapTracing = false;
    std::string tcpCongestionControl = "TcpNewReno";
    std::string applicationDataRate = "50Mbps";
    uint32_t payloadSize = 1448;
    std::string phyRate = "HtMcs7";
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcapTracing);
    cmd.AddValue("tcpCongestionControl", "TCP congestion control algorithm", tcpCongestionControl);
    cmd.AddValue("applicationDataRate", "Application data rate", applicationDataRate);
    cmd.AddValue("payloadSize", "Payload size", payloadSize);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.Parse(argc, argv);

    // Configure TCP congestion control algorithm
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpCongestionControl));

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNode;
    staNode.Create(1);

    // Configure YansWifiChannelHelper
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
    phy.SetChannel(channel.Create());

    // Configure WifiHelper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    // Configure WifiMacHelper
    WifiMacHelper mac;

    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    // Configure mobility models
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);

    // Install the internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNode);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // Configure TCP application
    uint16_t port = 50000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(apNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime + 1));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    onOffHelper.SetConstantRate(DataRate(applicationDataRate));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApp = onOffHelper.Install(staNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    // Set the PHY rate
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxGain", StringValue (phyRate));

    // Enable MPDU aggregation
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxAmsduSize", UintegerValue (7935));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxAmpduSize", UintegerValue (65535));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxSizeThreshold", UintegerValue (7991));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/AmsduEnable", BooleanValue (true));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/AmpduEnable", BooleanValue (true));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/FragmentEnable", BooleanValue (false));

    // Enable PCAP tracing
    if (enablePcapTracing) {
        phy.EnablePcap("wifi-tcp-aggregation", apDevice);
        phy.EnablePcap("wifi-tcp-aggregation", staDevice);
    }

    // Configure FlowMonitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // Analyze the results
    monitor->CheckForLostPackets();
    Ptr<Iprobe> iprobe = monitor->GetIprobe();
    iprobe->PrintFormatted(std::cout);

    Ptr<FlowClassifier> classifier = flowMonitor.GetClassifier();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    // Destroy the simulator
    Simulator::Destroy();

    return 0;
}