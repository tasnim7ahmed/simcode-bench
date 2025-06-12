#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Default parameters
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // Maximum AMPDU size in bytes
    double payloadSize = 1000;     // UDP payload size in bytes
    double simulationTime = 10.0;  // Simulation time in seconds
    double minExpectedThroughput = 0.0;
    double maxExpectedThroughput = 100.0;

    // Command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS mechanism", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum size of aggregated MPDUs (bytes)", maxAmpduSize);
    cmd.AddValue("payloadSize", "UDP payload size (bytes)", payloadSize);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("minExpectedThroughput", "Minimum expected throughput (Mbps)", minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput", "Maximum expected throughput (Mbps)", maxExpectedThroughput);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: AP and two stations
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Create channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup Wi-Fi
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    // Enable AMPDU aggregation
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"),
                                 "RtsCtsThreshold", UintegerValue(enableRtsCts ? 0 : 999999));

    // Set AMPDU attributes
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmpduSize", UintegerValue(maxAmpduSize));

    // Set up MAC layers
    Ssid ssid = Ssid("hidden-station-network");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility model to create hidden node scenario
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Set up UDP traffic from stations to AP
    uint16_t port = 9;

    // Server on AP
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    // Clients on stations
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // Infinite packets
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001))); // High rate
        echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    // PCAP tracing
    phy.EnablePcap("hidden_stations_ampdu", apDevice.Get(0));

    // ASCII tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("hidden_stations_ampdu.tr");
    phy.EnableAsciiAll(stream);

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate throughput
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRxBytes = 0;
    for (auto &[flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier())->FindFlow(flowId);
        if (t.destinationPort == port) {
            totalRxBytes += flowStats.rxBytes;
        }
    }

    double throughput = (totalRxBytes * 8.0) / (simulationTime * 1000000.0); // Mbps

    NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");
    NS_LOG_UNCOND("Expected range: [" << minExpectedThroughput << ", " << maxExpectedThroughput << "] Mbps");

    if (throughput < minExpectedThroughput || throughput > maxExpectedThroughput) {
        NS_LOG_ERROR("Throughput out of expected bounds!");
    }

    Simulator::Destroy();
    return 0;
}