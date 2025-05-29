#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiMpduAggregationExample");

Ptr<OutputStreamWrapper> g_stream;
uint64_t g_bytesTotal = 0;
uint64_t g_lastBytes = 0;

void
ThroughputCallback(Ptr<Application> app, double simTime, double interval)
{
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
    uint64_t currBytes = sink->GetTotalRx();
    uint64_t newBytes = currBytes - g_lastBytes;
    double throughputMbps = (newBytes * 8.0) / (interval * 1000000.0);
    g_stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughputMbps << std::endl;
    g_lastBytes = currBytes;
    if (Simulator::Now().GetSeconds() + interval <= simTime)
    {
        Simulator::Schedule(Seconds(interval), &ThroughputCallback, app, simTime, interval);
    }
}

int main(int argc, char *argv[])
{
    // Default parameters
    std::string tcpVariant = "ns3::TcpNewReno";
    std::string phyRate = "150Mbps";
    std::string dataRate = "50Mbps";
    uint32_t payloadSize = 1400;
    double simulationTime = 10.0;
    bool enablePcap = false;

    CommandLine cmd;
    cmd.AddValue("tcpVariant", "TCP congestion control (ns3::TcpNewReno, ns3::TcpCubic, ...)", tcpVariant);
    cmd.AddValue("dataRate", "OnOffApp DataRate (e.g. 10Mbps)", dataRate);
    cmd.AddValue("payloadSize", "OnOffApp Payload size (bytes)", payloadSize);
    cmd.AddValue("phyRate", "WiFi TX bitrate (e.g. 65Mbps, 150Mbps)", phyRate);
    cmd.AddValue("simulationTime", "Simulation duration (s)", simulationTime);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Set TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpVariant));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    // Nodes
    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    // WiFi: 802.11n 5GHz
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    // MPDU aggregation
    Config::SetDefault("ns3::WifiRemoteStationManager::UseMpduAggregation", BooleanValue(true));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmsduSize", UintegerValue(7935)); // Largest for 802.11n
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmpduSize", UintegerValue(65535)); // Largest for 802.11n

    NetDeviceContainer staDevices, apDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Set physical bitrate
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxGain", DoubleValue(0.0));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(18.0));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(18.0));
    Config::Set("/NodeList/*/DeviceList/*/RemoteStationManager/DataMode", StringValue("HtMcs7"));
    Config::SetDefault("ns3::WifiPhy::TxGain", DoubleValue(0.0));
    Config::SetDefault("ns3::WifiPhy::RxGain", DoubleValue(0.0));

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);
    // STA at (0,0,0), AP at (5,0,0)
    Ptr<MobilityModel> staMobility = wifiStaNode.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    staMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    apMobility->SetPosition(Vector(5.0, 0.0, 0.0));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf, apIf;
    staIf = address.Assign(staDevices);
    apIf = address.Assign(apDevices);

    // Set Wi-Fi bitrate (for all unicast traffic)
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("HtMcs7"));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/NonUnicastMode", StringValue("HtMcs0"));

    // TCP App: On STA, sending to AP
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(apIf.GetAddress(0), sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime + 1.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer app = onoff.Install(wifiStaNode.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime + 1.0));

    // Tracing
    if (enablePcap)
    {
        phy.EnablePcapAll("tcp-wifi-mpdu-aggregation", true);
    }

    // Report throughput every 100ms
    AsciiTraceHelper ascii;
    g_stream = ascii.CreateFileStream("throughput.txt");
    *g_stream->GetStream() << "Time(s)\tThroughput(Mbps)" << std::endl;
    Simulator::Schedule(Seconds(1.1), &ThroughputCallback, sinkApp.Get(0), simulationTime, 0.1);

    Simulator::Stop(Seconds(simulationTime + 2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}