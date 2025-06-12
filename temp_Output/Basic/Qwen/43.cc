#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenNodeScenario");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 4; // number of aggregated MPDUs
    uint32_t payloadSize = 1000; // bytes
    double simulationTime = 5.0; // seconds
    double expectedThroughputLow = 0.0;
    double expectedThroughputHigh = 100.0; // Mbps

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum A-MPDU size (number of MPDUs)", maxAmpduSize);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("expectedThroughputLow", "Expected minimum throughput (Mbps)", expectedThroughputLow);
    cmd.AddValue("expectedThroughputHigh", "Expected maximum throughput (Mbps)", expectedThroughputHigh);
    cmd.Parse(argc, argv);

    // Set up Wi-Fi helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("1000"));
    }

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    phy.SetChannel(channel.Create());

    // Limit wireless range to 5 meters
    Ptr<RangePropagationLossModel> lossModel = CreateObject<RangePropagationLossModel>();
    lossModel->SetAttribute("MaxRange", DoubleValue(5.0));
    phy.AddPropagationLoss(lossModel);

    WifiMacHelper mac;
    Ssid ssid = Ssid("hidden-node-network");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice;
    NodeContainer apNode;
    apNode.Create(1);
    apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    NodeContainer staNodes;
    staNodes.Create(2);
    staDevices = wifi.Install(phy, mac, staNodes);

    // Enable AMPDU with specified aggregation size
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/MaxAmpduSize", UintegerValue(maxAmpduSize * 1500));

    // Mobility: place nodes at 5.5m distance to ensure hidden node scenario
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.5),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes + apNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(apNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;
    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    // UDP applications from stations to AP
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("1000Mb/s"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        apps.Add(onoff.Install(staNodes.Get(i)));
    }
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime + 1.0));

    // Packet sink on AP
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(apNode);
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(simulationTime + 1.0));

    // Tracing
    phy.EnablePcap("hidden_node_ap", apDevice.Get(0));
    phy.EnablePcap("hidden_node_sta1", staDevices.Get(0));
    phy.EnablePcap("hidden_node_sta2", staDevices.Get(1));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("hidden_node.tr"));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1.5));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStats stats = monitor->GetFlowStats().begin()->second;

    double rxBytes = stats.rxBytes * 8.0 / (simulationTime * 1000000.0); // Mbps
    NS_LOG_UNCOND("Throughput: " << rxBytes << " Mbps");
    NS_LOG_UNCOND("Expected range: [" << expectedThroughputLow << ", " << expectedThroughputHigh << "] Mbps");

    if (rxBytes >= expectedThroughputLow && rxBytes <= expectedThroughputHigh) {
        NS_LOG_UNCOND("Throughput is within expected bounds.");
    } else {
        NS_LOG_ERROR("Throughput is out of expected bounds!");
    }

    Simulator::Destroy();
    return 0;
}