#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHiddenNodesSimulation");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // Maximum size of aggregated MPDUs
    uint32_t payloadSize = 1472;   // Default UDP payload size (bytes)
    double simulationTime = 10.0;  // Simulation duration in seconds
    double expectedThroughputLow = 0.0;  // Lower bound for throughput check
    double expectedThroughputHigh = 100.0; // Upper bound for throughput check

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS mechanism", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum size of A-MPDU (bytes)", maxAmpduSize);
    cmd.AddValue("payloadSize", "UDP Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("throughputLow", "Expected minimum throughput (Mbps)", expectedThroughputLow);
    cmd.AddValue("throughputHigh", "Expected maximum throughput (Mbps)", expectedThroughputHigh);
    cmd.Parse(argc, argv);

    // Set up Wi-Fi with 802.11n and MPDU aggregation
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    // Enable MPDU aggregation
    wifiHelper.SetHtConfiguration(CreateObject<DefaultHtConfiguration>());
    wifiHelper.SetGreenfieldEnabled(true);
    wifiHelper.SetShortGuardIntervalEnabled(true);

    if (enableRtsCts) {
        wifiHelper.SetRtsCtsThreshold(100); // Enable RTS/CTS for packets larger than this threshold
    } else {
        wifiHelper.SetRtsCtsThreshold(0xffffffff); // Disable RTS/CTS
    }

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Limit wireless range to ~5 meters using propagation loss
    wifiPhy.Set("TxPowerStart", DoubleValue(5));
    wifiPhy.Set("TxPowerEnd", DoubleValue(5));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("LossModelName", StringValue("ns3::RangePropagationLossModel"));
    wifiPhy.Set("MaxRange", DoubleValue(5));

    // Create nodes: AP + two hidden stations
    NodeContainer wifiStaNodes;
    NodeContainer wifiApNode;
    wifiStaNodes.Create(2);
    wifiApNode.Create(1);

    NodeContainer allNodes = NodeContainer(wifiApNode, wifiStaNodes);

    // Install devices
    NetDeviceContainer staDevices;
    NetDeviceContainer apDevice;

    // Configure MAC for stations
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(Ssid("hidden-network")),
                    "ActiveProbing", BooleanValue(false));
    staDevices = wifiHelper.Install(wifiPhy, wifiMac, wifiStaNodes);

    // Configure MAC for AP
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(Ssid("hidden-network")),
                    "BeaconInterval", TimeValue(Seconds(2.5)),
                    "EnableBeaconJitter", BooleanValue(false));
    apDevice = wifiHelper.Install(wifiPhy, wifiMac, wifiApNode);

    // Set AMPDU attributes
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtCapabilities/HtSupported", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Ammpdu", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/MaxAmpduSize", UintegerValue(maxAmpduSize));

    // Mobility model - place nodes out of range of each other
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(1.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Assign positions to create a hidden node scenario
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // AP at origin
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));     // STA1
    positionAlloc->Add(Vector(0.0, 5.0, 0.0));     // STA2 (hidden from STA1)
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(allNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // UDP applications
    uint16_t port = 9;

    // Server on AP
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    // Clients on stations sending to AP
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // Unlimited
        client.SetAttribute("Interval", TimeValue(Seconds(0.001)));    // Saturated flow
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    // Tracing
    wifiPhy.EnablePcapAll("wifi-hidden-nodes");
    AsciiTraceHelper asciiTraceHelper;
    wifiPhy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wifi-hidden-nodes.tr"));

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.Install(allNodes);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate throughput
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRxBytes = 0;

    for (auto [flowId, stat] : stats) {
        Ipv4FlowClassifier::FiveTuple t = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier())->FindFlow(flowId);
        if (t.destinationPort == port && t.protocol == 17) { // UDP to AP
            totalRxBytes += stat.rxBytes;
        }
    }

    double throughput = (totalRxBytes * 8.0) / (simulationTime * 1e6); // Mbps
    NS_LOG_UNCOND("Measured Throughput: " << throughput << " Mbps");
    NS_LOG_UNCOND("Expected Throughput Range: [" << expectedThroughputLow << ", " << expectedThroughputHigh << "] Mbps");

    if (throughput >= expectedThroughputLow && throughput <= expectedThroughputHigh) {
        NS_LOG_UNCOND("Throughput within expected bounds.");
    } else {
        NS_LOG_ERROR("Throughput OUTSIDE expected bounds!");
    }

    Simulator::Destroy();
    return 0;
}