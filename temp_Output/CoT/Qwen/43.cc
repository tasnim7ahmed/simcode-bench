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
    uint32_t maxAmpduSize = 4095;
    uint32_t payloadSize = 1024;
    double simulationTime = 10.0;
    double expectedThroughputLow = 0.0;
    double expectedThroughputHigh = 1e9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum AMPDU size in bytes", maxAmpduSize);
    cmd.AddValue("payloadSize", "UDP Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("expectedThroughputLow", "Minimum expected throughput (bps)", expectedThroughputLow);
    cmd.AddValue("expectedThroughputHigh", "Maximum expected throughput (bps)", expectedThroughputHigh);
    cmd.Parse(argc, argv);

    // Create nodes: AP and two stations
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Setup physical environment
    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(2.4e9));
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));
    phy.Set("RxGain", DoubleValue(0.0));

    // Setup Wi-Fi MAC and PHY
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    if (enableRtsCts) {
        wifi.Set("RTSThreshold", UintegerValue(0));
    } else {
        wifi.Set("RTSThreshold", UintegerValue(2347));
    }

    // Enable MPDU aggregation
    wifi.SetBlockAckThresholds(2, 2, 0, 0);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("hidden-network")),
                "BeaconInterval", TimeValue(Seconds(2)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("hidden-network")),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility to create hidden node scenario
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

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // UDP traffic setup
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApp1 = clientHelper.Install(wifiStaNodes.Get(0));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(simulationTime));

    ApplicationContainer clientApp2 = clientHelper.Install(wifiStaNodes.Get(1));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(simulationTime));

    // PCAP tracing
    phy.EnablePcapAll("hidden_node_simulation");

    // ASCII tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("hidden_node_simulation.tr");
    phy.EnableAsciiAll(stream);

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationPort == 9) {
            totalThroughput += i->second.rxBytes * 8.0 / simulationTime;
        }
    }

    NS_LOG_UNCOND("Total Throughput: " << totalThroughput / 1e6 << " Mbps");
    NS_LOG_UNCOND("Expected Throughput Range: [" << expectedThroughputLow / 1e6 << ", " << expectedThroughputHigh / 1e6 << "] Mbps");

    if (totalThroughput >= expectedThroughputLow && totalThroughput <= expectedThroughputHigh) {
        NS_LOG_UNCOND("Throughput is within expected bounds.");
    } else {
        NS_LOG_ERROR("Throughput is out of expected bounds!");
    }

    Simulator::Destroy();
    return 0;
}