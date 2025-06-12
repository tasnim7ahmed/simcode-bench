#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211nSimulation");

int main(int argc, char *argv[]) {
    uint32_t numStations = 5;
    uint8_t htMcs = 7;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    double distance = 1.0; // meters
    bool enableRtsCts = false;
    Time simulationTime = Seconds(10);

    CommandLine cmd(__FILE__);
    cmd.AddValue("numStations", "Number of stations", numStations);
    cmd.AddValue("htMcs", "HT MCS value (0-7)", htMcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("distance", "Distance between nodes in meters", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (true/false)", enableRtsCts);
    cmd.Parse(argc, argv);

    if (htMcs > 7) {
        NS_FATAL_ERROR("HT MCS should be between 0 and 7");
    }
    if (channelWidth != 20 && channelWidth != 40) {
        NS_FATAL_ERROR("Channel width must be either 20 or 40 MHz");
    }

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numStations);
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

    if (enableRtsCts) {
        mac.Set("RTSThreshold", UintegerValue(0));
    } else {
        mac.Set("RTSThreshold", UintegerValue(999999));
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
    // Set guard interval
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported",
                BooleanValue(shortGuardInterval));

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

    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numStations; ++i) {
        UdpEchoServerHelper echoServer(port + i);
        serverApps.Add(echoServer.Install(wifiApNode.Get(0)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simulationTime);

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numStations; ++i) {
        UdpEchoClientHelper echoClient(staInterfaces.GetAddress(i), port + i);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(echoClient.Install(wifiApNode.Get(0)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simulationTime);
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    for (auto flow : stats) {
        if (flow.first.m_sourceId != flow.first.m_sinkId) { // exclude loopback
            totalThroughput += (flow.second.rxBytes * 8.0) / simulationTime.GetSeconds() / 1e6;
        }
    }

    std::cout << "Aggregated UDP Throughput: " << totalThroughput << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}