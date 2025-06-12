#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

WifiStandard ParseStandard(const std::string& version) {
    if (version == "80211a") return WIFI_STANDARD_80211a;
    if (version == "80211b") return WIFI_STANDARD_80211b;
    if (version == "80211g") return WIFI_STANDARD_80211g;
    if (version == "80211n") return WIFI_STANDARD_80211n;
    if (version == "80211ac") return WIFI_STANDARD_80211ac;
    if (version == "80211ax") return WIFI_STANDARD_80211ax;
    NS_ABORT_MSG("Unknown Wi-Fi standard: " << version);
    return WIFI_STANDARD_UNSPECIFIED;
}

WifiPhyBand ParseBand(const std::string& version) {
    if (version.find("a") != std::string::npos || version.find("ac") != std::string::npos || version.find("ax") != std::string::npos) {
        return WIFI_PHY_BAND_5GHZ;
    }
    return WIFI_PHY_BAND_2_4GHZ;
}

int main(int argc, char* argv[]) {
    std::string apVersion = "80211ac";
    std::string staVersion = "80211n";
    std::string remoteIp = "192.168.1.2";
    uint16_t port = 9;
    Time simulationTime = Seconds(10.0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("apVersion", "Wi-Fi standard for AP (e.g., 80211ac)", apVersion);
    cmd.AddValue("staVersion", "Wi-Fi standard for STA (e.g., 80211n)", staVersion);
    cmd.AddValue("remoteIp", "Remote IP address for traffic destination", remoteIp);
    cmd.AddValue("port", "Port number for UDP traffic", port);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(ParseStandard(apVersion));
    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");

    Ssid ssid = Ssid("backward-compat-network");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    wifi.SetStandard(ParseStandard(staVersion));
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    MobilityHelper mobility;
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

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

    GlobalRouteManager::PopulateRoutingTables();

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simulationTime);

    UdpClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(wifiApNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime);

    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    double throughput = static_cast<double>(server.GetReceived()) * 8 / (simulationTime.GetSeconds() * 1000000);
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

    return 0;
}