#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Tcp80211nMpduAggregation");

static uint64_t g_bytesTotal = 0;
static Ptr<PacketSink> g_sinkApp;
static Time g_lastTime = Seconds(0);

void
ThroughputPrinter()
{
    Time now = Simulator::Now();
    double throughput = (g_bytesTotal * 8.0) / (now - g_lastTime).GetSeconds() / 1e6; // Mbps
    std::cout << now.GetSeconds() << "s\tThroughput: " << throughput << " Mbps" << std::endl;
    g_bytesTotal = 0;
    g_lastTime = now;
    Simulator::Schedule(MilliSeconds(100), &ThroughputPrinter);
}

void
SinkRxCallback(Ptr<const Packet> packet, const Address &)
{
    g_bytesTotal += packet->GetSize();
}

int main(int argc, char *argv[])
{
    std::string tcpCongControl = "ns3::TcpNewReno";
    std::string appDataRate = "50Mbps";
    uint32_t payloadSize = 1448;
    std::string phyBitrate = "150Mbps";
    double simDuration = 10.0;
    bool enablePcap = false;

    CommandLine cmd;
    cmd.AddValue("tcpCongControl", "TCP Congestion Control Algorithm", tcpCongControl);
    cmd.AddValue("appDataRate", "Application Data Rate", appDataRate);
    cmd.AddValue("payloadSize", "TCP Payload Size (bytes)", payloadSize);
    cmd.AddValue("phyBitrate", "Physical layer bitrate", phyBitrate);
    cmd.AddValue("simDuration", "Simulation duration (seconds)", simDuration);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpCongControl));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ChannelWidth", UintegerValue(40)); // 802.11n usually uses 20/40 MHz

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n-mpdu");

    // Enable MPDU aggregation
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    phy.Set("TxPowerStart", DoubleValue(16.0));
    phy.Set("TxPowerEnd", DoubleValue(16.0));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false),
                "BE_MaxAmsduSize", UintegerValue(7935),
                "BE_MaxAmpduSize", UintegerValue(65535));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BE_MaxAmsduSize", UintegerValue(7935),
                "BE_MaxAmpduSize", UintegerValue(65535));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set the physical Wi-Fi bitrate
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxProfileList/0/DataTxVector/Mode",
                StringValue("HtMcs7"));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxProfileList/0/DataTxVector/ChannelWidth",
                UintegerValue(40));

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfc, apIfc;
    staIfc = address.Assign(staDevice);
    apIfc = address.Assign(apDevice);

    // Application: BulkSend from STA to AP
    uint16_t port = 5001;
    BulkSendHelper bulk("ns3::TcpSocketFactory",
                        InetSocketAddress(apIfc.GetAddress(0), port));
    bulk.SetAttribute("MaxBytes", UintegerValue(0));
    bulk.SetAttribute("SendSize", UintegerValue(payloadSize));

    // Respect app data rate
    bulk.SetAttribute("Saturation", BooleanValue(true));
    bulk.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer app = bulk.Install(wifiStaNode.Get(0));
    app.Start(Seconds(0.5));
    app.Stop(Seconds(simDuration));

    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simDuration));

    g_sinkApp = DynamicCast<PacketSink>(sinkApp.Get(0));
    g_lastTime = Seconds(0);

    // Connect throughput measurement
    g_sinkApp->TraceConnectWithoutContext("Rx", MakeCallback(&SinkRxCallback));
    Simulator::Schedule(Seconds(0.1), &ThroughputPrinter);

    if (enablePcap)
    {
        phy.EnablePcap("tcp-80211n-mpdu-ap", apDevice.Get(0));
        phy.EnablePcap("tcp-80211n-mpdu-sta", staDevice.Get(0));
    }

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}