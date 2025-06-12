#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleWifiInfrastructure");

int main(int argc, char *argv[]) {
    std::string phyMode("DsssRate1Mbps");
    double rss = -80; // dBm
    uint32_t packetSize = 1000; // bytes
    uint32_t numPackets = 20;
    double interval = 1.0; // seconds
    bool verbose = false;
    bool pcapTracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "number of packets to send", numPackets);
    cmd.AddValue("interval", "interval (seconds) between packets", interval);
    cmd.AddValue("verbose", "turn on all WifiNetDevice log components", verbose);
    cmd.AddValue("pcap", "enable PCAP tracing", pcapTracing);
    cmd.Parse(argc, argv);

    // Convert interval to Time object
    Time interPacketInterval = Seconds(interval);

    // Configure the PHY and channel
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper phy;
    phy.Set("TxPowerStart", DoubleValue(15.0));
    phy.Set("TxPowerEnd", DoubleValue(15.0));
    phy.Set("RxGain", DoubleValue(0.0));

    // Loss model: Fixed RSS
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
    lossModel->SetRss(rss);
    phy.SetChannelParameter("LossModel", PointerValue(lossModel));

    WifiMacHelper mac;
    Ssid ssid = Ssid("simple-infra");

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create channel and devices
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // AP node
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, nodes.Get(0));

    // Station node
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, nodes.Get(1));

    // Mobility model (not used for signal strength but required)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // UDP Echo client-server setup
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable logging
    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Tracing
    if (pcapTracing) {
        phy.EnablePcap("simple-wifi-infra-ap", apDevice.Get(0));
        phy.EnablePcap("simple-wifi-infra-sta", staDevice.Get(0));
    }

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}