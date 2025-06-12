#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Tcp80211nAggregationExample");

static uint64_t lastTotalRx = 0;

void
ThroughputMonitor(Ptr<PacketSink> sink, Time interval)
{
    static double lastTime = 0.0;
    Time now = Simulator::Now();
    double curTime = now.GetSeconds();
    uint64_t curTotalRx = sink->GetTotalRx();
    double throughput = (curTotalRx - lastTotalRx) * 8.0 / (1000000.0 * (curTime - lastTime));
    std::cout << now.GetSeconds() << "s: Throughput: " << throughput << " Mbit/s" << std::endl;
    lastTotalRx = curTotalRx;
    lastTime = curTime;
    Simulator::Schedule(interval, &ThroughputMonitor, sink, interval);
}

int main(int argc, char *argv[])
{
    std::string tcpTypeId = "ns3::TcpNewReno";
    std::string dataRate = "100Mbps";
    uint32_t payloadSize = 1460;
    std::string phyBitrate = "150Mbps";
    double simulationTime = 10.0;
    bool enablePcap = false;

    CommandLine cmd;
    cmd.AddValue("tcpCongestion", "TCP congestion control algorithm (e.g., ns3::TcpNewReno, ns3::TcpCubic, ns3::TcpBbr)", tcpTypeId);
    cmd.AddValue("dataRate", "OnOffApp DataRate", dataRate);
    cmd.AddValue("payloadSize", "Application payload size (bytes)", payloadSize);
    cmd.AddValue("phyBitrate", "802.11n physical layer bitrate (e.g., 150Mbps)", phyBitrate);
    cmd.AddValue("simulationTime", "Duration of simulation in seconds", simulationTime);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    SeedManager::SetSeed(1);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    // Enable MPDU aggregation
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    phy.Set("ChannelWidth", UintegerValue(40));
    phy.Set("ShortGuardEnabled", BooleanValue(false));
    phy.Set("TxPowerStart", DoubleValue(18.0));
    phy.Set("TxPowerEnd", DoubleValue(18.0));
    phy.Set("Antennas", UintegerValue(1));
    phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(1));
    phy.SetErrorRateModel("ns3::NistErrorRateModel");

    // Set aggregation parameters
    Config::SetDefault("ns3::HtWifiMac::MPDUAggregator", StringValue("ns3::MpduAggregator"));
    Config::SetDefault("ns3::MpduAggregator::MaxNbMpdu", UintegerValue(64));
    Config::SetDefault("ns3::MpduAggregator::MaxAmsduSize", UintegerValue(7935));

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));
    posAlloc->Add(Vector(3.0, 0.0, 0.0));
    mobility.SetPositionAllocator(posAlloc);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf = address.Assign(staDevices);
    Ipv4InterfaceContainer apIf = address.Assign(apDevices);

    uint16_t port = 9;
    Address apLocalAddress(InetSocketAddress(apIf.GetAddress(0), port));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime + 1));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", apLocalAddress);
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

    ApplicationContainer clientApp = clientHelper.Install(wifiStaNode.Get(0));

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    lastTotalRx = 0;

    Simulator::Schedule(Seconds(1.1), &ThroughputMonitor, sink, Seconds(0.1));

    if (enablePcap)
    {
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.EnablePcap("tcp-80211n-ap", apDevices.Get(0));
        phy.EnablePcap("tcp-80211n-sta", staDevices.Get(0));
    }

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}