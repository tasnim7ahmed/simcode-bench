#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosThroughputSimulation");

int main(int argc, char *argv[])
{
    uint32_t nStations = 5;
    uint8_t htMcs = 7;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    double distance = 1.0; // meters
    bool enableRtsCts = false;
    Time simulationTime = Seconds(10.0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("htMcs", "HT MCS value (0-7)", htMcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("distance", "Distance between stations and AP (meters)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (true/false)", enableRtsCts);
    cmd.Parse(argc, argv);

    if (htMcs > 7)
    {
        NS_LOG_ERROR("HT MCS must be between 0 and 7.");
        return 1;
    }

    if (channelWidth != 20 && channelWidth != 40)
    {
        NS_LOG_ERROR("Channel width must be either 20 or 40 MHz.");
        return 1;
    }

    // Set up logging
    LogComponentEnable("WifiTosThroughputSimulation", LOG_LEVEL_INFO);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("HtMcs" + std::to_string(htMcs)),
                                  "ControlMode", StringValue("HtMcs" + std::to_string(htMcs)));

    // Enable/disable RTS/CTS
    if (enableRtsCts)
    {
        wifiStaMac.Set("RtsCtsEnabled", BooleanValue(true));
        wifiApMac.Set("RtsCtsEnabled", BooleanValue(true));
    }

    // Configure HT options
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("HtMcs" + std::to_string(htMcs)));
    Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
    Config::SetDefault("ns3::WifiPhy::ShortGuardInterval", BooleanValue(shortGuardInterval));

    // Setup MAC layers
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP server on AP
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simulationTime);

    // UDP clients on all stations
    UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295u)); // unlimited
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = clientHelper.Install(wifiStaNodes);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime);

    Simulator::Stop(simulationTime);
    Simulator::Run();

    double totalRxBytes = server.GetServer()->GetReceived();
    double throughput = (totalRxBytes * 8) / (simulationTime.GetSeconds() * 1e6);

    NS_LOG_INFO("Total received bytes: " << totalRxBytes);
    NS_LOG_INFO("Throughput: " << throughput << " Mbps");

    Simulator::Destroy();

    return 0;
}