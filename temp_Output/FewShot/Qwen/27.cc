#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsGoodputSimulation");

int main(int argc, char *argv[]) {
    // Simulation parameters with defaults
    uint8_t mcsValue = 0;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = true;
    double simulationTime = 10.0;
    std::string dataMode = "udp";
    bool enableRtsCts = false;
    double distance = 1.0;
    double expectedThroughput = 0.0;

    // Command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("mcs", "MCS value (0-7)", mcsValue);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("time", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataMode", "Transport mode: udp or tcp", dataMode);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake (true/false)", enableRtsCts);
    cmd.AddValue("distance", "Distance between station and AP in meters", distance);
    cmd.AddValue("expectedThroughput", "Expected throughput for comparison (Mbps)", expectedThroughput);
    cmd.Parse(argc, argv);

    // Validate MCS value
    NS_ABORT_MSG_IF(mcsValue > 7, "Invalid MCS value. Must be between 0 and 7.");

    // Validate channel width
    NS_ABORT_MSG_IF(channelWidth != 20 && channelWidth != 40, "Channel width must be either 20 or 40 MHz.");

    // Enable logging
    LogComponentEnable("WifiMcsGoodputSimulation", LOG_LEVEL_INFO);

    // Create nodes (AP + STA)
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Create channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    // Install Wi-Fi devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    // Set up rates
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate" + std::to_string(mcsValue)),
                                 "ControlMode", StringValue("OfdmRate3"));

    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiMacQueueItem::MaxPacketNumber", UintegerValue(1));
    }

    // Set channel width and guard interval
    Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
    Config::SetDefault("ns3::WifiPhy::ShortGuardInterval", BooleanValue(shortGuardInterval));

    NetDeviceContainer staDevices;
    NetDeviceContainer apDevice;

    staDevices = wifi.Install(phy, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Server port
    uint16_t port = 9;

    // Application setup
    ApplicationContainer serverApp;
    ApplicationContainer clientApp;

    if (dataMode == "udp") {
        // UDP Echo Server on AP
        UdpEchoServerHelper echoServer(port);
        serverApp = echoServer.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime));

        // UDP Echo Client on STA
        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // unlimited
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        clientApp = echoClient.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    } else if (dataMode == "tcp") {
        // TCP Bulk Send Server on AP
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApp = sinkHelper.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime));

        // TCP Bulk Send Client on STA
        BulkSendHelper sender("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
        sender.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

        clientApp = sender.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    } else {
        NS_FATAL_ERROR("Invalid data mode. Use 'udp' or 'tcp'.");
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate results
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRx = 0;
    for (auto &[flowId, flowStats] : stats) {
        if (flowId == 2) { // Only consider the client-to-server flow
            totalRx = flowStats.rxBytes * 8.0 / (simulationTime * 1000000); // Mbps
            NS_LOG_INFO("Received " << flowStats.rxBytes << " bytes over " << simulationTime << " seconds [RX rate] = " << totalRx << " Mbps");
        }
    }

    // Compare with expected throughput
    if (expectedThroughput > 0) {
        double diff = std::abs(totalRx - expectedThroughput);
        NS_LOG_INFO("Difference from expected throughput (" << expectedThroughput << " Mbps): " << diff << " Mbps");
    }

    Simulator::Destroy();

    return 0;
}