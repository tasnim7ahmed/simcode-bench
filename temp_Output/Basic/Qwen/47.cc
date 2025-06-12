#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuse80211axExperiment");

int main(int argc, char *argv[]) {
    double distanceAp1Sta1 = 5.0;
    double distanceAp2Sta2 = 5.0;
    double distanceBetweenAps = 20.0;
    uint32_t packetSize = 1472; // bytes
    Time interval = MilliSeconds(100); // packets every 100ms
    double txPowerAp = 16.02; // dBm
    double txPowerSta = 16.02; // dBm
    double ccaEdThreshold = -82.0; // dBm
    double obssPdThreshold = -80.0; // dBm
    bool enableObssPd = true;
    uint32_t simulationTime = 10; // seconds
    uint32_t channelWidth = 20; // MHz

    CommandLine cmd(__FILE__);
    cmd.AddValue("distanceAp1Sta1", "Distance between AP1 and STA1 (m)", distanceAp1Sta1);
    cmd.AddValue("distanceAp2Sta2", "Distance between AP2 and STA2 (m)", distanceAp2Sta2);
    cmd.AddValue("distanceBetweenAps", "Distance between AP1 and AP2 (m)", distanceBetweenAps);
    cmd.AddValue("packetSize", "Size of application packets sent (bytes)", packetSize);
    cmd.AddValue("interval", "Interval between packets (ms)", interval);
    cmd.AddValue("txPowerAp", "Transmit power for APs (dBm)", txPowerAp);
    cmd.AddValue("txPowerSta", "Transmit power for STAs (dBm)", txPowerSta);
    cmd.AddValue("ccaEdThreshold", "CCA-ED threshold in dBm", ccaEdThreshold);
    cmd.AddValue("obssPdThreshold", "OBSS-PD threshold in dBm", obssPdThreshold);
    cmd.AddValue("enableObssPd", "Enable or disable OBSS-PD spatial reuse", enableObssPd);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
    Config::SetDefault("ns3::WifiPhy::ChannelSettings", StringValue("{0, 0MHz, BAND_5GHZ, 0}"));
    Config::SetDefault("ns3::WifiPhy::TxPowerStart", DoubleValue(txPowerAp));
    Config::SetDefault("ns3::WifiPhy::TxPowerEnd", DoubleValue(txPowerAp));
    Config::SetDefault("ns3::WifiPhy::CcaEdThreshold", DoubleValue(ccaEdThreshold));
    Config::SetDefault("ns3::HeConfiguration::EnableMuBar", BooleanValue(true));

    if (enableObssPd) {
        Config::SetDefault("ns3::WifiPhy::EnableObssPd", BooleanValue(true));
        Config::SetDefault("ns3::WifiPhy::ObssPdLevel", DoubleValue(obssPdThreshold));
    } else {
        Config::SetDefault("ns3::WifiPhy::EnableObssPd", BooleanValue(false));
    }

    NodeContainer apNodes;
    apNodes.Create(2);
    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());
    phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));

    WifiMacHelper macHelper;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ax_5GHZ);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("HeMcs0"),
                                       "ControlMode", StringValue("HeMcs0"));

    Ssid ssid1 = Ssid("BSS1");
    Ssid ssid2 = Ssid("BSS2");

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "BeaconInterval", TimeValue(Seconds(2)));
    NetDeviceContainer apDevices1 = wifiHelper.Install(phyHelper, macHelper, apNodes.Get(0));

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "BeaconInterval", TimeValue(Seconds(2)));
    NetDeviceContainer apDevices2 = wifiHelper.Install(phyHelper, macHelper, apNodes.Get(1));

    phyHelper.Set("TxPowerStart", DoubleValue(txPowerSta));
    phyHelper.Set("TxPowerEnd", DoubleValue(txPowerSta));

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid1));
    NetDeviceContainer staDevices1 = wifiHelper.Install(phyHelper, macHelper, staNodes.Get(0));

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid2));
    NetDeviceContainer staDevices2 = wifiHelper.Install(phyHelper, macHelper, staNodes.Get(1));

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP1
    positionAlloc->Add(Vector(distanceBetweenAps, 0.0, 0.0)); // AP2
    positionAlloc->Add(Vector(distanceAp1Sta1, 0.0, 0.0)); // STA1
    positionAlloc->Add(Vector(distanceBetweenAps - distanceAp2Sta2, 0.0, 0.0)); // STA2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);

    address.NewNetwork();
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    uint16_t port = 9;
    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces1.GetAddress(0), port));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff1.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

    ApplicationContainer apps1 = onoff1.Install(staNodes.Get(0));
    apps1.Start(Seconds(1.0));
    apps1.Stop(Seconds(simulationTime));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces2.GetAddress(0), port));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

    ApplicationContainer apps2 = onoff2.Install(staNodes.Get(1));
    apps2.Start(Seconds(1.0));
    apps2.Stop(Seconds(simulationTime));

    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps1 = sink1.Install(apNodes.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(simulationTime));

    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps2 = sink2.Install(apNodes.Get(1));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    double throughput1 = (sinkApps1.Get(0)->GetObject<PacketSink>()->GetTotalRx() * 8.0) / (simulationTime * 1000000.0);
    double throughput2 = (sinkApps2.Get(0)->GetObject<PacketSink>()->GetTotalRx() * 8.0) / (simulationTime * 1000000.0);

    std::ofstream logFile("spatial_reuse_log.txt", std::ios_base::app);
    logFile << "Simulation Parameters: "
            << "Distance AP1-STA1=" << distanceAp1Sta1 << ", "
            << "Distance AP2-STA2=" << distanceAp2Sta2 << ", "
            << "Distance AP1-AP2=" << distanceBetweenAps << ", "
            << "TX Power AP=" << txPowerAp << ", TX Power STA=" << txPowerSta << ", "
            << "CCA-ED Threshold=" << ccaEdThreshold << ", OBSS-PD Threshold=" << obssPdThreshold << ", "
            << "OBSS-PD Enabled=" << enableObssPd << ", Channel Width=" << channelWidth << " MHz" << std::endl;

    logFile << "Throughput BSS1: " << throughput1 << " Mbps" << std::endl;
    logFile << "Throughput BSS2: " << throughput2 << " Mbps" << std::endl;
    logFile << "---------------------------------------------" << std::endl;
    logFile.close();

    Simulator::Destroy();

    return 0;
}