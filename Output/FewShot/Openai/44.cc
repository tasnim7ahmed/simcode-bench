#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/propagation-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiFixedRssExample");

int main(int argc, char *argv[]) {
    double rss = -80.0; // dBm
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue("rss", "Fixed RSS value (dBm)", rss);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets", numPackets);
    cmd.AddValue("interval", "Packet interval (s)", interval);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("WifiFixedRssExample", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<FixedRssLossModel> rssModel = CreateObject<FixedRssLossModel> ();
    rssModel->SetRss(rss);
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.SetPropagationLossModel(rssModel);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));

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

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    phy.EnablePcapAll("wifi-fixed-rss");

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}