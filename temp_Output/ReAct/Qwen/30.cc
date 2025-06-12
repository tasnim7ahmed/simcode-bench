#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosThroughputSimulation");

int main(int argc, char *argv[])
{
    uint32_t nStations = 4;
    uint8_t mcs = 7;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    double distance = 1.0; // meters
    bool enableRtsCts = false;
    Time simulationTime = Seconds(10);

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("mcs", "HT MCS value (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("distance", "Distance between nodes and AP (meters)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (true/false)", enableRtsCts);
    cmd.AddValue("simulationTime", "Duration of the simulation", simulationTime);
    cmd.Parse(argc, argv);

    if (mcs > 7)
    {
        std::cerr << "MCS must be between 0 and 7" << std::endl;
        return 1;
    }
    if (channelWidth != 20 && channelWidth != 40)
    {
        std::cerr << "Channel width must be either 20 or 40 MHz" << std::endl;
        return 1;
    }

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    phy.SetErrorRateModel("ns3::YansErrorRateModel");
    phy.Set("ChannelSettings", StringValue("{0, " + std::to_string(channelWidth) + ", 0}"));

    // Configure remote station manager for HT
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                                 "ControlMode", StringValue("HtMcs" + std::to_string(mcs)));

    // Station MACs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));

    if (enableRtsCts)
    {
        Config::SetDefault("ns3::WifiMac::RTSThreshold", UintegerValue(0));
    }

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(nStations),
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
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP Server on AP
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime);

    // UDP Clients on all stations
    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        clientApps.Add(client.Install(wifiStaNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime);

    // Flow monitor to calculate throughput
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simulationTime);
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalRxBytes = 0;
    for (auto& [flowId, flowStats] : stats)
    {
        if (flowStats.rxPackets > 0)
        {
            totalRxBytes += flowStats.rxBytes;
        }
    }

    double throughput = (totalRxBytes * 8.0) / (simulationTime.GetSeconds() * 1e6); // Mbps

    std::cout.precision(3);
    std::cout << "Throughput: " << std::fixed << throughput << " Mbps | "
              << "Stations: " << nStations << " | "
              << "MCS: " << static_cast<uint32_t>(mcs) << " | "
              << "Channel Width: " << channelWidth << "MHz | "
              << "Guard Interval: " << (shortGuardInterval ? "Short" : "Long") << " | "
              << "RTS/CTS: " << (enableRtsCts ? "Enabled" : "Disabled") << std::endl;

    Simulator::Destroy();
    return 0;
}