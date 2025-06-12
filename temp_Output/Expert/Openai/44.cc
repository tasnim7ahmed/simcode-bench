#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiFixedRssInfraExample");

int main(int argc, char *argv[])
{
    double rss = -80.0;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0;
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
    cmd.AddValue("packetSize", "Size of packets sent (bytes)", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets (s)", interval);
    cmd.AddValue("verbose", "Enable verbose log output", verbose);

    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("WifiFixedRssInfraExample", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<FixedRssLossModel> rssLossModel = CreateObject<FixedRssLossModel>();
    rssLossModel->SetRss(rss);
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> chan = channel.Create();
    chan->SetPropagationLossModel(rssLossModel);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(chan);
    phy.Set("TxPowerStart", DoubleValue(16.0));
    phy.Set("TxPowerEnd", DoubleValue(16.0));
    phy.Set("EnergyDetectionThreshold", DoubleValue(-96.0));
    phy.Set("CcaMode1Threshold", DoubleValue(-99.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    Ptr<MobilityModel> staMobility = wifiStaNode.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    staMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    apMobility->SetPosition(Vector(1.0, 0.0, 0.0));

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);

    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(100.0));

    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(apInterfaces.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("1Mbps"), packetSize);
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(1.0 + interval * numPackets)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onoff.Install(wifiStaNode.Get(0));

    phy.EnablePcap("wifi_fixed_rss_sta", staDevice.Get(0));
    phy.EnablePcap("wifi_fixed_rss_ap", apDevice.Get(0));

    Simulator::Stop(Seconds(2.0 + interval * numPackets));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}