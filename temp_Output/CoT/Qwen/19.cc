#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBackwardCompatibilitySimulation");

WifiStandard GetStandardFromVersion(const std::string& version, WifiPhyBand* band) {
    if (version == "80211a") {
        *band = WIFI_PHY_BAND_5GHZ;
        return WIFI_STANDARD_80211a;
    } else if (version == "80211b") {
        *band = WIFI_PHY_BAND_2_4GHZ;
        return WIFI_STANDARD_80211b;
    } else if (version == "80211g") {
        *band = WIFI_PHY_BAND_2_4GHZ;
        return WIFI_STANDARD_80211g;
    } else if (version == "80211n") {
        *band = WIFI_PHY_BAND_2_4GHZ; // Can also be 5GHz
        return WIFI_STANDARD_80211n;
    } else if (version == "80211ac") {
        *band = WIFI_PHY_BAND_5GHZ;
        return WIFI_STANDARD_80211ac;
    } else if (version == "80211ax") {
        *band = WIFI_PHY_BAND_5GHZ; // or 2.4GHz
        return WIFI_STANDARD_80211ax;
    } else {
        NS_FATAL_ERROR("Unknown Wi-Fi standard: " << version);
    }
    return WIFI_STANDARD_UNSPECIFIED;
}

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    std::string apVersion = "80211ac";
    std::string staVersion = "80211n";
    std::string phyModeAp = "OfdmRate24Mbps";
    std::string phyModeSta = "ErpOfdmRate24Mbps";
    std::string remoteIp = "192.168.1.2";
    uint16_t port = 9;
    double simulationTime = 10.0;
    bool enableMobility = true;
    cmd.AddValue("apVersion", "Wi-Fi standard for AP (e.g., 80211a, 80211ac)", apVersion);
    cmd.AddValue("staVersion", "Wi-Fi standard for STA (e.g., 80211n, 80211ax)", staVersion);
    cmd.AddValue("remoteIp", "Remote IP address for UDP client", remoteIp);
    cmd.AddValue("port", "Port number to use", port);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("enableMobility", "Enable node mobility", enableMobility);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiPhyBand apBand, staBand;
    WifiStandard apStandard = GetStandardFromVersion(apVersion, &apBand);
    WifiStandard staStandard = GetStandardFromVersion(staVersion, &staBand);

    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(apStandard);
    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
    phy.Set("ChannelNumber", UintegerValue(36));
    phy.Set("PhyBand", EnumValue(apBand));
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    wifi.SetStandard(staStandard);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
    phy.Set("ChannelNumber", UintegerValue(1));
    phy.Set("PhyBand", EnumValue(staBand));
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    MobilityHelper mobility;
    if (enableMobility) {
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(wifiApNode);
        mobility.Install(wifiStaNodes);
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiApNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}