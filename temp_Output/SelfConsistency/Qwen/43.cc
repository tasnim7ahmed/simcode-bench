#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddennodesAggregation");

int main(int argc, char *argv[])
{
    uint32_t nStations = 2;
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // bytes
    uint32_t payloadSize = 1472;   // bytes
    double simulationTime = 10.0;  // seconds
    double expectedThroughputLow = 0.0;
    double expectedThroughputHigh = 100.0; // Mbps

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum AMPDU size (bytes)", maxAmpduSize);
    cmd.AddValue("payloadSize", "Payload size per packet (bytes)", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("expectedThroughputLow", "Minimum expected throughput (Mbps)", expectedThroughputLow);
    cmd.AddValue("expectedThroughputHigh", "Maximum expected throughput (Mbps)", expectedThroughputHigh);
    cmd.Parse(argc, argv);

    if (payloadSize > maxAmpduSize)
    {
        NS_FATAL_ERROR("Payload size cannot exceed maximum AMPDU size");
    }

    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    // Enable AMPDU aggregation
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(2200));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(enableRtsCts ? 0 : 2200));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue(10));

    Ssid ssid = Ssid("hidden-node-network");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility: place all nodes within a small area to simulate hidden nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Set the transmission range to ~5 meters
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(5));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/txPowerBase", DoubleValue(5));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/txPowerLevels", UintegerValue(1));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(20));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardInterval", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize", UintegerValue(maxAmpduSize));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;
    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    // UDP traffic from stations to AP
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinks;
    for (uint32_t i = 0; i < nStations; ++i)
    {
        sinkHelper.SetAttribute("Protocol", TypeIdValue(UdpSocketFactory::GetTypeId()));
        sinks.Add(sinkHelper.Install(wifiApNode.Get(0)));
    }
    sinks.Start(Seconds(0.0));
    sinks.Stop(Seconds(simulationTime + 0.1));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mb/s"))); // Saturated flow

    ApplicationContainer apps;
    for (uint32_t i = 0; i < nStations; ++i)
    {
        apps.Add(onoff.Install(wifiStaNodes.Get(i)));
    }
    apps.Start(Seconds(0.1));
    apps.Stop(Seconds(simulationTime));

    // Tracing
    phy.EnablePcapAll("wifi-hidden-node-ampdu");
    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wifi-hidden-node-ampdu.tr"));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 0.5));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationPort == port)
        {
            totalThroughput += (i->second.rxBytes * 8.0) / simulationTime / 1e6;
        }
    }

    NS_LOG_UNCOND("Total Throughput: " << totalThroughput << " Mbps");
    if (totalThroughput >= expectedThroughputLow && totalThroughput <= expectedThroughputHigh)
    {
        NS_LOG_UNCOND("Throughput is within expected bounds.");
    }
    else
    {
        NS_LOG_WARN("Throughput is OUTSIDE expected bounds!");
    }

    Simulator::Destroy();
    return 0;
}