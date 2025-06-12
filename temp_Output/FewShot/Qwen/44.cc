#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    std::string phyMode("DsssRate1Mbps");
    double rss = -80; // dBm
    uint32_t packetSize = 1000;
    uint32_t numPackets = 20;
    double interval = 1.0; // seconds
    bool verbose = false;
    bool pcapTracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("rss", "received signal strength", rss);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "number of packets to send", numPackets);
    cmd.AddValue("interval", "interval (seconds) between packets", interval);
    cmd.AddValue("verbose", "turn on all Wifi logging", verbose);
    cmd.AddValue("pcap", "enable PCAP tracing", pcapTracing);
    cmd.Parse(argc, argv);

    // Convert interval to Time object
    Time interPacketInterval = Seconds(interval);

    // Disable fragmentation for tiny packets
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
    // Turn off RTS/CTS for small packets
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
    // Set fixed RSS model
    Config::SetDefault("ns3::PropagationLossModel::LossExponent", DoubleValue(0));
    Config::SetDefault("ns3::PropagationLossModel::MinDistance", DoubleValue(1.0));

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("WifiMacQueueItem", LOG_LEVEL_ALL);
        LogComponentEnable("WifiNetDevice", LOG_LEVEL_ALL);
        LogComponentEnable("ApWifiMac", LOG_LEVEL_ALL);
        LogComponentEnable("StaWifiMac", LOG_LEVEL_ALL);
        LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
        LogComponentEnable("WifiChannel", LOG_LEVEL_ALL);
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    // Setup AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Setup STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Mobility models
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    Ipv4InterfaceContainer apInterface;

    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // Add static route from station to access point
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create a OnOff application sending UDP packets from STA to AP
    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apInterface.GetAddress(0), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(1000000))); // 1 Mbps
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(numPackets * packetSize));

    ApplicationContainer clientApp = onoff.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Sink to receive packets at AP
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Tracing
    if (pcapTracing) {
        phy.EnablePcap("wifi-ap", apDevice.Get(0));
        phy.EnablePcap("wifi-sta", staDevice.Get(0));
    }

    // Set fixed RSS
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetRss(rss);
    phy.GetPhy(0)->SetLossModel(lossModel);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}