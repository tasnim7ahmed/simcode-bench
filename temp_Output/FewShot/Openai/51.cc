#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiAggregation");

static Ptr<PacketSink> sinkApp;
static uint64_t lastTotalRx = 0;

void
ThroughputMonitor(Time interval)
{
    Time now = Simulator::Now();
    uint64_t curTotalRx = sinkApp->GetTotalRx();
    double throughput = (curTotalRx - lastTotalRx) * 8.0 / (interval.GetSeconds() * 1e6); // Mbps
    std::cout << now.GetSeconds() << "s\tThroughput: " << throughput << " Mbps" << std::endl;
    lastTotalRx = curTotalRx;
    Simulator::Schedule(interval, &ThroughputMonitor, interval);
}

int main(int argc, char *argv[])
{
    std::string tcpType = "TcpNewReno";
    std::string appDataRate = "50Mbps";
    uint32_t payloadSize = 1460;
    std::string phyBitrate = "150Mbps";
    double simTime = 10.0;
    bool enablePcap = false;

    CommandLine cmd;
    cmd.AddValue("tcpType", "TCP congestion control (e.g., TcpNewReno, TcpCubic, TcpBbr)", tcpType);
    cmd.AddValue("appDataRate", "Application data rate", appDataRate);
    cmd.AddValue("payloadSize", "Application payload size (bytes)", payloadSize);
    cmd.AddValue("phyBitrate", "PHY bitrate (e.g., 150Mbps)", phyBitrate);
    cmd.AddValue("simTime", "Simulation duration (seconds)", simTime);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // TCP Congestion Algorithm
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpType));

    // Wifi Helpers
    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    // Set MPDU aggregation
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("999999"));
    Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue("HtMcs7"));
    Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue("HtMcs0"));
    phy.Set("ShortGuardEnabled", BooleanValue(true));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    // Enable MPDU aggregation
    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("512p"));
    Config::SetDefault("ns3::QosTxop::QueueSize", StringValue("512p"));
    Config::SetDefault("ns3::HtWifiMac::BE_MaxAmpduSize", UintegerValue(65535));

    NetDeviceContainer staDevice, apDevice;

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set PHY data rate
    for (uint32_t i = 0; i < staDevice.GetN(); ++i)
    {
        staDevice.Get(i)->SetAttribute("DataRate", StringValue(phyBitrate));
    }
    for (uint32_t i = 0; i < apDevice.GetN(); ++i)
    {
        apDevice.Get(i)->SetAttribute("DataRate", StringValue(phyBitrate));
    }

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);
    wifiStaNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    wifiApNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 0.0, 0.0));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);

    // Applications: TCP Bulk Send from STA to AP
    uint16_t port = 5000;
    Address apLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", apLocalAddress);
    ApplicationContainer sinkAppCont = sinkHelper.Install(wifiApNode.Get(0));
    sinkApp = StaticCast<PacketSink>(sinkAppCont.Get(0));
    sinkAppCont.Start(Seconds(0.0));
    sinkAppCont.Stop(Seconds(simTime + 1));

    BulkSendHelper source("ns3::TcpSocketFactory",
                          InetSocketAddress(apInterfaces.GetAddress(0), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    source.SetAttribute("SendSize", UintegerValue(payloadSize));
    source.SetAttribute("Remote", AddressValue(InetSocketAddress(apInterfaces.GetAddress(0), port)));

    ApplicationContainer sourceApp = source.Install(wifiStaNode.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(simTime + 1));

    // Set app data rate via socket buffer
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(2 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(2 * 1024 * 1024));

    // Enable PCAP if needed
    if (enablePcap)
    {
        phy.EnablePcap("tcp-agg-ap", apDevice.Get(0));
        phy.EnablePcap("tcp-agg-sta", staDevice.Get(0));
    }

    // Throughput reporting
    Simulator::Schedule(Seconds(1.1), &ThroughputMonitor, MilliSeconds(100));

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}