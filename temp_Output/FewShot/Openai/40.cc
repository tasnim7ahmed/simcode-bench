#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocFixedRssWifiExample");

int main(int argc, char *argv[])
{
    double rss = -80.0; // dBm
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    bool verbose = true;
    double rxThreshold = -81.0; // dBm, default, below which packets are not received.
    std::string phyMode = "DsssRate11Mbps";
    std::string pcapFilePrefix = "adhoc-80211b-rss";

    CommandLine cmd;
    cmd.AddValue("rss", "Fixed received signal strength (dBm)", rss);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("rxThreshold", "Receiver Sensitivity Threshold (dBm)", rxThreshold);
    cmd.AddValue("phyMode", "Wifi Phy mode (e.g. DsssRate11Mbps)", phyMode);
    cmd.AddValue("pcapFilePrefix", "Prefix for pcap trace files", pcapFilePrefix);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("AdhocFixedRssWifiExample", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Wifi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);

    // Fixed RSS loss model
    Ptr<YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel>();
    Ptr<FixedRssLossModel> rssModel = CreateObject<FixedRssLossModel>();
    rssModel->SetRss(rss);
    wifiChannel->SetPropagationLossModel(rssModel);
    wifiChannel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    wifiPhy.SetChannel(wifiChannel);

    // Set receiver threshold for energy detection (in linear units)
    // dBm to linear W: 10^(dBm/10) / 1000
    double rxThresholdW = std::pow(10.0, rxThreshold / 10.0) / 1000.0;
    wifiPhy.Set("RxSensitivity", DoubleValue(rxThresholdW));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install server (PacketSink) on node 1
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Install client (OnOffApplication) on node 0
    OnOffHelper client("ns3::UdpSocketFactory", sinkAddress);
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(5.0));

    // Enable pcap tracing
    wifiPhy.EnablePcap(pcapFilePrefix + "-node0", devices.Get(0));
    wifiPhy.EnablePcap(pcapFilePrefix + "-node1", devices.Get(1));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}