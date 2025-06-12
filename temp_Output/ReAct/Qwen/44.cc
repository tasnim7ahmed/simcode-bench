#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInfrastructureSimulation");

int main(int argc, char *argv[]) {
    std::string phyMode("DsssRate1Mbps");
    double rss = -80; // dBm
    uint32_t packetSize = 1000; // bytes
    uint32_t numPackets = 20;
    double interval = 1.0; // seconds
    bool verbose = false;
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("rss", "Received Signal Strength", rss);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "number of packets to send", numPackets);
    cmd.AddValue("interval", "interval (seconds) between packets", interval);
    cmd.AddValue("verbose", "turn on all Wifi logging", verbose);
    cmd.AddValue("tracing", "enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    // Convert interval to Time object
    Time interPacketInterval = Seconds(interval);

    // Set up logging
    if (verbose) {
        LogComponentEnable("WifiMacQueueItem", LOG_LEVEL_ALL);
        LogComponentEnable("WifiMacQueue", LOG_LEVEL_ALL);
        LogComponentEnable("WifiNetDevice", LOG_LEVEL_ALL);
        LogComponentEnable("ApWifiMac", LOG_LEVEL_ALL);
        LogComponentEnable("StaWifiMac", LOG_LEVEL_ALL);
        LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
        LogComponentEnable("WifiRemoteStationManager", LOG_LEVEL_ALL);
    }

    // Create nodes: one AP and one station
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    // Create channels
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    // Setup PHY and MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Set up FixedRssLossModel
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetRss(rss);
    wifiChannel->AddPropagationLoss(lossModel);

    // Set propagation delay
    wifiChannel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    WifiPhyHelper phy;
    phy.SetChannel(wifiChannel);

    WifiMacHelper mac;

    // Install AP
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install STA
    mac.SetType("ns3::StaWifiMac");
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Add IP stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevice);

    // Set up client-server communication
    uint16_t port = 9;

    // Server (AP)
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // Client (STA)
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000000bps"))); // irrelevant due to interval

    ApplicationContainer clientApp = onoff.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Schedule packet sending with custom interval
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetStream(0);

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                    MakeCallback([](Ptr<const Packet> p, const Address &) {
                        NS_LOG_INFO("Received packet of size " << p->GetSize());
                    }));

    if (tracing) {
        phy.EnablePcap("wifi_infra_ap", apDevice.Get(0));
        phy.EnablePcap("wifi_infra_sta", staDevice.Get(0));
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}