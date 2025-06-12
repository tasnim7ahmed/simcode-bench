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
    static void ParseWifiVersion(std::string versionStr, WifiStandard* standard, WifiPhyBand* band) {
        if (versionStr == "80211a") {
            *standard = WIFI_STANDARD_80211a;
            *band = WIFI_PHY_BAND_5GHZ;
        } else if (versionStr == "80211b") {
            *standard = WIFI_STANDARD_80211b;
            *band = WIFI_PHY_BAND_2_4GHZ;
        } else if (versionStr == "80211g") {
            *standard = WIFI_STANDARD_80211g;
            *band = WIFI_PHY_BAND_2_4GHZ;
        } else if (versionStr == "80211n") {
            *standard = WIFI_STANDARD_80211n;
            *band = WIFI_PHY_BAND_2_4GHZ; // Can be either 2.4 or 5 GHz
        } else if (versionStr == "80211ac") {
            *standard = WIFI_STANDARD_80211ac;
            *band = WIFI_PHY_BAND_5GHZ;
        } else if (versionStr == "80211ax") {
            *standard = WIFI_STANDARD_80211ax;
            *band = WIFI_PHY_BAND_2_4GHZ; // Or 5 GHz depending on configuration
        } else {
            NS_ABORT_MSG("Unknown Wi-Fi version: " << versionStr);
        }
    }
};

int main(int argc, char *argv[]) {
    std::string apWifiVersion = "80211ac";
    std::string staWifiVersion = "80211n";
    std::string remoteIp = "192.168.1.1";
    uint16_t port = 9;
    double simTime = 10.0;
    bool enableMobility = true;
    std::string rateControlAlgorithmAp = "ns3::MinstrelWifiManager";
    std::string rateControlAlgorithmSta = "ns3::AarfcdWifiManager";

    CommandLine cmd(__FILE__);
    cmd.AddValue("apWifiVersion", "Wi-Fi version for AP (e.g., 80211a, 80211ac)", apWifiVersion);
    cmd.AddValue("staWifiVersion", "Wi-Fi version for STA (e.g., 80211n)", staWifiVersion);
    cmd.AddValue("remoteIp", "Remote IP address for UDP traffic", remoteIp);
    cmd.AddValue("port", "Port number for UDP traffic", port);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("enableMobility", "Enable node mobility", enableMobility);
    cmd.AddValue("rateControlAlgorithmAp", "Rate control algorithm for AP", rateControlAlgorithmAp);
    cmd.AddValue("rateControlAlgorithmSta", "Rate control algorithm for STA", rateControlAlgorithmSta);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyAp;
    YansWifiPhyHelper phySta;

    WifiStandard apStandard, staStandard;
    WifiPhyBand apBand, staBand;

    WifiVersionHelper::ParseWifiVersion(apWifiVersion, &apStandard, &apBand);
    WifiVersionHelper::ParseWifiVersion(staWifiVersion, &staStandard, &staBand);

    phyAp.SetChannel(channel.Create());
    phyAp.Set("Standard", EnumValue(apStandard));
    phyAp.Set("ChannelNumber", UintegerValue(36)); // 5 GHz
    phyAp.Set("RxGain", DoubleValue(0));
    phyAp.Set("TxPowerStart", DoubleValue(16.02));
    phyAp.Set("TxPowerEnd", DoubleValue(16.02));

    phySta.SetChannel(phyAp.GetChannel());
    phySta.Set("Standard", EnumValue(staStandard));
    phySta.Set("ChannelNumber", UintegerValue(1)); // 2.4 GHz
    phySta.Set("RxGain", DoubleValue(0));
    phySta.Set("TxPowerStart", DoubleValue(16.02));
    phySta.Set("TxPowerEnd", DoubleValue(16.02));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-backward-compat");

    WifiHelper wifiAp;
    wifiAp.SetRemoteStationManager(rateControlAlgorithmAp.c_str());

    WifiHelper wifiSta;
    wifiSta.SetRemoteStationManager(rateControlAlgorithmSta.c_str());

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice = wifiAp.Install(phyAp, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifiSta.Install(phySta, mac, wifiStaNodes);

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
    } else {
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }

    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    double throughput = server.GetThroughput() / 1e6;
    NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");

    Simulator::Destroy();

    return 0;
}