#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiThroughputTest");

int main(int argc, char *argv[]) {
    uint32_t nStations = 5;
    uint8_t htMcs = 7;
    uint16_t channelWidth = 40;
    bool shortGuardInterval = true;
    double distance = 10.0; // meters
    bool enableRtsCts = false;
    Time simulationTime = Seconds(10);

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("htMcs", "HT MCS value (0-7)", htMcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("distance", "Distance between nodes and AP (meters)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (true/false)", enableRtsCts);
    cmd.Parse(argc, argv);

    if (htMcs > 7) {
        std::cerr << "HT MCS must be between 0 and 7" << std::endl;
        return 1;
    }

    if (channelWidth != 20 && channelWidth != 40) {
        std::cerr << "Channel width must be either 20 or 40 MHz" << std::endl;
        return 1;
    }

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("TxPowerEnd", DoubleValue(20));
    phy.Set("TxPowerStart", DoubleValue(20));

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(htMcs)),
                                 "ControlMode", StringValue("HtMcs" + std::to_string(htMcs)));

    if (enableRtsCts) {
        mac.SetRTSThreshold(1000);
    } else {
        mac.SetRTSThreshold(999999);
    }

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns3-80211n")),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns3-80211n")));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set channel width
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(channelWidth));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardInterval", BooleanValue(shortGuardInterval));

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

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;
    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;

    for (uint32_t i = 0; i < nStations; ++i) {
        packetSinkHelper.SetAttribute("Protocol", TypeIdValue(UdpSocketFactory::GetTypeId()));
        sinkApps.Add(packetSinkHelper.Install(wifiStaNodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(simulationTime);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));

    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < nStations; ++i) {
        AddressValue remoteAddress(InetSocketAddress(staInterfaces.GetAddress(i), port));
        onoff.SetAttribute("Remote", remoteAddress);
        sourceApps.Add(onoff.Install(wifiApNode.Get(0)));
    }
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(simulationTime - Seconds(1.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simulationTime);
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRxBytes = 0;

    for (auto& [flowId, flowStats] : stats) {
        totalRxBytes += flowStats.rxBytes;
    }

    double throughput = (totalRxBytes * 8) / (simulationTime.GetSeconds() * 1e6);
    std::cout.precision(4);
    std::cout << "Throughput: " << std::fixed << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}