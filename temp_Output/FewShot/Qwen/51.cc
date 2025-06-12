#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Default simulation parameters
    std::string tcpVariant = "TcpNewReno";
    std::string dataRate = "100Mbps";
    uint32_t payloadSize = 1448;
    std::string phyRate("HtMcs7");
    double simulationTime = 10.0;
    bool enablePcap = false;

    // Parse command-line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "TCP Congestion Control Algorithm (e.g., TcpNewReno, TcpCubic)", tcpVariant);
    cmd.AddValue("dataRate", "Application Data Rate (e.g., 100Mbps)", dataRate);
    cmd.AddValue("payloadSize", "TCP Payload Size in bytes", payloadSize);
    cmd.AddValue("phyRate", "Wi-Fi Physical Layer Bitrate (e.g., HtMcs7 for 65 Mbps, HtMcs15 for 150 Mbps)", phyRate);
    cmd.AddValue("simulationTime", "Total Simulation Time in seconds", simulationTime);
    cmd.AddValue("enablePcap", "Enable PCAP Tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Set TCP congestion control algorithm
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));
    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid), "Unknown TCP variant: " << tcpVariant);
    Config::SetDefault("ns3::TcpSocketFactory::SocketType", TypeIdValue(tcpTid));

    // Create nodes: one AP and one STA
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Install AP
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyRate), "ControlMode", StringValue("HtMcs0"));
    wifi.SetMac(mac);
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));
    phy.Set("TxPowerLevels", UintegerValue(1));
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("CcaMode1Threshold", DoubleValue(-79));
    phy.Set("EnergyDetectionThreshold", DoubleValue(-89 + 3));

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Install STA
    NetDeviceContainer staDevice;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Server application on AP
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    // Client application on STA
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    bulkSend.SetAttribute("SendSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp = bulkSend.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(0.1));
    clientApp.Stop(Seconds(simulationTime));

    // Enable MPDU Aggregation
    Config::SetDefault("ns3::WifiRemoteStationManager::AggregateQueue", BooleanValue(true));
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(MilliSeconds(100)));

    // Enable Pcap if needed
    if (enablePcap) {
        phy.EnablePcap("tcp_wireless_simulation_ap", apDevice.Get(0));
        phy.EnablePcap("tcp_wireless_simulation_sta", staDevice.Get(0));
    }

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Throughput tracking every 100ms
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> throughputStream = asciiTraceHelper.CreateFileStream("throughput-trace.txt");
    Simulator::Schedule(Seconds(0.1), &ThroughputMonitor, monitor, throughputStream, MilliSeconds(100));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    monitor->CheckForLostPackets();
    Simulator::Destroy();

    return 0;
}

void ThroughputMonitor(Ptr<FlowMonitor> monitor, Ptr<OutputStreamWrapper> stream, Time interval, Time nextWrite) {
    if (nextWrite > Simulator::Now()) {
        Simulator::Schedule(interval, &ThroughputMonitor, monitor, stream, nextWrite);
        return;
    }
    nextWrite += interval;
    double totalRxBytes = 0;
    monitor->GetAllFlows();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        totalRxBytes += iter->second.rxBytes;
    }
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << (totalRxBytes * 8.0 / interval.GetSeconds()) / 1e6 << std::endl;
    totalRxBytes = 0;
    Simulator::Schedule(interval, &ThroughputMonitor, monitor, stream, nextWrite);
}