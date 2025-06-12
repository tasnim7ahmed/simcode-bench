#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMcsThroughputTest");

int
main(int argc, char *argv[])
{
    uint32_t mcsIndex = 1;
    double distance = 1.0; // meters
    double simulationTime = 10; // seconds
    std::string phyMode("Spectrum");
    std::string errorModelType("ns3::WifiRadioEnergyModel");
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    bool enableAggregation = true;
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("mcsIndex", "MCS index value (0-7 for HT, up to 9 for VHT)", mcsIndex);
    cmd.AddValue("distance", "Distance in meters between STA and AP", distance);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("phyMode", "Phy mode type: Yans or Spectrum", phyMode);
    cmd.AddValue("errorModelType", "Error model implementation type", errorModelType);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80, etc.)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("enableAggregation", "Enable MPDU/MSDU aggregation", enableAggregation);
    cmd.AddValue("verbose", "Turn on all log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("WiFiMcsThroughputTest", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phyYans;
    SpectrumWifiPhyHelper phySpectrum;

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("VhtMcs" + std::to_string(mcsIndex)),
                                 "ControlMode", StringValue("VhtMcs" + std::to_string(mcsIndex)));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-mcs-test");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    if (phyMode == "Yans")
    {
        phyYans = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channelYans = YansWifiChannelHelper::Default();
        phyYans.SetChannel(channelYans.Create());
        phyYans.Set("ChannelWidth", UintegerValue(channelWidth));
        phyYans.SetErrorRateModel(errorModelType);
        staDevices = wifi.Install(phyYans, mac, wifiStaNode);
    }
    else
    {
        phySpectrum = SpectrumWifiPhyHelper::Default();
        Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
        Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
        spectrumChannel->AddPropagationLossModel(lossModel);
        Ptr<ConstantPositionMobilityModel> apMobility = CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel>();
        wifiApNode.Get(0)->AggregateObject(apMobility);
        wifiStaNode.Get(0)->AggregateObject(staMobility);
        staMobility->SetPosition(Vector(distance, 0.0, 0.0));
        apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
        phySpectrum.SetChannel(spectrumChannel);
        phySpectrum.Set("ChannelWidth", UintegerValue(channelWidth));
        phySpectrum.SetErrorRateModel(errorModelType);
        staDevices = wifi.Install(phySpectrum, mac, wifiStaNode);
    }

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    if (enableAggregation)
    {
        mac.Set("BE_MaxAmpduSize", UintegerValue(65535));
        mac.Set("BK_MaxAmpduSize", UintegerValue(65535));
        mac.Set("VI_MaxAmpduSize", UintegerValue(65535));
        mac.Set("VO_MaxAmpduSize", UintegerValue(65535));
    }

    NetDeviceContainer apDevices;
    if (phyMode == "Yans")
    {
        apDevices = wifi.Install(phyYans, mac, wifiApNode);
    }
    else
    {
        apDevices = wifi.Install(phySpectrum, mac, wifiApNode);
    }

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterface;

    apInterface = address.Assign(apDevices);
    staInterface = address.Assign(staDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(staInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiApNode.Get(0));
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(simulationTime - 0.1));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t totalBytes = 0;
    for (auto& socket : serverApps.Get(0)->GetObject<UdpEchoServer>()->GetReceivedList())
    {
        totalBytes += socket.second.size();
    }

    double throughput = (totalBytes * 8) / (simulationTime * 1000000.0);

    std::cout << "MCS Index: " << mcsIndex
              << ", Channel Width: " << channelWidth << "MHz"
              << ", Guard Interval: " << (shortGuardInterval ? "Short" : "Long")
              << ", Throughput: " << throughput << " Mbps"
              << ", Distance: " << distance << " m"
              << ", Phy Mode: " << phyMode
              << std::endl;

    Simulator::Destroy();
    return 0;
}