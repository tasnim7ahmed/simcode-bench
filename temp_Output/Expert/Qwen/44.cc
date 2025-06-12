#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleRssBasedSimulation");

int main(int argc, char *argv[])
{
    double rss = -80; // dBm
    uint32_t packetSize = 1024;
    uint32_t numPackets = 20;
    double interval = 1.0; // seconds
    bool verbose = false;
    bool pcapTracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
    cmd.AddValue("packetSize", "Size of application packets in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("verbose", "Enable verbose logging output", verbose);
    cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("WifiSimpleRssBasedSimulation", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    phy.Set("ChannelWidth", UintegerValue(20));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("simple-wifi-rss-network");

    // Station
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    // Access Point
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Mobility model for both nodes (fixed position)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // RSS configuration
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetRss(rss);
    phy.GetPhy(0)->SetLossModel(lossModel);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevices);

    // UDP Echo server on AP
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));

    // UDP Echo client on station
    UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    // Enable PCAP tracing
    if (pcapTracing)
    {
        phy.EnablePcap("wifi-simple-rss-based-sta", staDevices.Get(0));
        phy.EnablePcap("wifi-simple-rss-based-ap", apDevices.Get(0));
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}