#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBackwardCompatibilitySimulation");

class WifiVersionHelper {
public:
    static WifiStandard GetStandardFromVersion(const std::string& version) {
        if (version == "80211a") return WIFI_STANDARD_80211a;
        else if (version == "80211b") return WIFI_STANDARD_80211b;
        else if (version == "80211g") return WIFI_STANDARD_80211g;
        else if (version == "80211n") return WIFI_STANDARD_80211n;
        else if (version == "80211ac") return WIFI_STANDARD_80211ac;
        else if (version == "80211ax") return WIFI_STANDARD_80211ax;
        else {
            NS_ABORT_MSG("Unknown Wi-Fi standard: " << version);
        }
    }

    static WifiPhyBand GetBandFromVersion(const std::string& version) {
        if (version == "80211a" || version == "80211ac" || version == "80211ax") {
            return WIFI_PHY_BAND_5GHZ;
        } else {
            return WIFI_PHY_BAND_2_4GHZ;
        }
    }
};

int main(int argc, char *argv[]) {
    std::string apVersion = "80211ac";
    std::string staVersion = "80211n";
    std::string raAlgorithm = "ns3::MinstrelWifiManager";
    Time simulationTime = Seconds(10.0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("apVersion", "Wi-Fi standard for AP (e.g., 80211a, 80211ac)", apVersion);
    cmd.AddValue("staVersion", "Wi-Fi standard for STA (e.g., 80211n, 80211ax)", staVersion);
    cmd.AddValue("raAlgorithm", "Rate adaptation algorithm", raAlgorithm);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiStandard apStandard = WifiVersionHelper::GetStandardFromVersion(apVersion);
    WifiStandard staStandard = WifiVersionHelper::GetStandardFromVersion(staVersion);

    WifiPhyBand apBand = WifiVersionHelper::GetBandFromVersion(apVersion);
    WifiPhyBand staBand = WifiVersionHelper::GetBandFromVersion(staVersion);

    phy.Set("Standard", WifiPhyHelper::GetStaticStandard(apStandard));
    phy.Set("ChannelNumber", UintegerValue(36));
    phy.Set("ChannelWidth", UintegerValue(20));
    phy.Set("PhyBand", EnumValue(apBand));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-compat-network");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));

    WifiHelper wifi;
    wifi.SetRemoteStationManager(raAlgorithm.c_str());
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    phy.Set("Standard", WifiPhyHelper::GetStaticStandard(staStandard));
    phy.Set("ChannelNumber", UintegerValue(1));
    phy.Set("ChannelWidth", UintegerValue(20));
    phy.Set("PhyBand", EnumValue(staBand));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

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
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    GlobalRouteManager::PopulateRoutingTables();

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simulationTime);

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(wifiApNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime);

    Simulator::Stop(simulationTime);
    Simulator::Run();

    double throughput = server.GetThroughput();
    NS_LOG_UNCOND("Throughput: " << throughput / 1e6 << " Mbps");

    Simulator::Destroy();
    return 0;
}