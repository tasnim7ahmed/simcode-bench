#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBackwardCompatibilitySimulation");

WifiStandard
StringToWifiStandard(const std::string& standard)
{
    if (standard == "80211a")
        return WIFI_STANDARD_80211a;
    else if (standard == "80211b")
        return WIFI_STANDARD_80211b;
    else if (standard == "80211g")
        return WIFI_STANDARD_80211g;
    else if (standard == "80211n")
        return WIFI_STANDARD_80211n;
    else if (standard == "80211ac")
        return WIFI_STANDARD_80211ac;
    else if (standard == "80211ax")
        return WIFI_STANDARD_80211ax;
    else
    {
        NS_ABORT_MSG("Unknown Wi-Fi standard: " << standard);
        return WIFI_STANDARD_UNSPECIFIED;
    }
}

WifiPhyBand
StringToWifiBand(const std::string& band)
{
    if (band == "2.4GHz")
        return WIFI_PHY_BAND_2_4GHZ;
    else if (band == "5GHz")
        return WIFI_PHY_BAND_5GHZ;
    else if (band == "6GHz")
        return WIFI_PHY_BAND_6GHZ;
    else
    {
        NS_ABORT_MSG("Unknown Wi-Fi band: " << band);
        return WIFI_PHY_BAND_UNSPECIFIED;
    }
}

int
main(int argc, char* argv[])
{
    std::string apStandardStr = "80211ac";
    std::string staStandardStr = "80211n";
    std::string apBandStr = "5GHz";
    std::string staBandStr = "2.4GHz";
    std::string raaAlgorithmAp = "ns3::MinstrelWifiManager";
    std::string raaAlgorithmSta = "ns3::AarfcdWifiManager";
    bool enableMobility = true;
    uint32_t payloadSize = 1472; // bytes
    Time simulationTime = Seconds(10);

    CommandLine cmd(__FILE__);
    cmd.AddValue("apStandard", "Wi-Fi standard for AP (e.g., 80211ac)", apStandardStr);
    cmd.AddValue("staStandard", "Wi-Fi standard for STA (e.g., 80211n)", staStandardStr);
    cmd.AddValue("apBand", "Wi-Fi band for AP (e.g., 5GHz)", apBandStr);
    cmd.AddValue("staBand", "Wi-Fi band for STA (e.g., 2.4GHz)", staBandStr);
    cmd.AddValue("raaAp", "Rate adaptation algorithm for AP", raaAlgorithmAp);
    cmd.AddValue("raaSta", "Rate adaptation algorithm for STA", raaAlgorithmSta);
    cmd.AddValue("enableMobility", "Enable node mobility", enableMobility);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.Parse(argc, argv);

    WifiStandard apStandard = StringToWifiStandard(apStandardStr);
    WifiStandard staStandard = StringToWifiStandard(staStandardStr);
    WifiPhyBand apBand = StringToWifiBand(apBandStr);
    WifiPhyBand staBand = StringToWifiBand(staBandStr);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyAp;
    phyAp.SetChannel(channel.Create());
    phyAp.Set("ChannelNumber", UintegerValue(36));
    phyAp.Set("PhyBand", EnumValue(apBand));

    YansWifiPhyHelper phySta;
    phySta.SetChannel(channel.Create());
    phySta.Set("ChannelNumber", UintegerValue(1));
    phySta.Set("PhyBand", EnumValue(staBand));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-backward-compat");

    WifiHelper wifiAp;
    wifiAp.SetStandard(apStandard);
    wifiAp.SetRemoteStationManager("ns3::ConstantRateWifiManager");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifiAp.Install(phyAp, mac, wifiApNode);

    WifiHelper wifiSta;
    wifiSta.SetStandard(staStandard);
    wifiSta.SetRemoteStationManager(raaAlgorithmSta);
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifiSta.Install(phySta, mac, wifiStaNodes);

    if (enableMobility)
    {
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(1.0),
                                      "DeltaY", DoubleValue(1.0),
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
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

    UdpServerHelper server(4000);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime);

    UdpClientHelper client(staInterface.GetAddress(0), 4000);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u)); // infinite
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(simulationTime);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(simulationTime);
    Simulator::Run();

    double throughput =
        server.GetServer()->GetTotalRx() * 8.0 / simulationTime.GetSeconds() / 1000 / 1000;
    NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");

    Simulator::Destroy();
    return 0;
}