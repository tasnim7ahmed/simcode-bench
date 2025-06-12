#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpAggregation");

void CalculateThroughput() {
    static Time lastTime = Seconds(0);
    Time currentTime = Simulator::Now();
    double interval = (currentTime - lastTime).GetSeconds();
    uint64_t rxBytes = 0;

    FlowMonitor* flowMon = Simulator::GetObject<FlowMonitor>(0);
    if (flowMon == nullptr) {
        return;
    }

    FlowMonitor::FlowStatsContainer stats = flowMon->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        rxBytes += i->second.rxBytes;
    }

    double throughput = (rxBytes * 8.0) / (interval * 1000000.0); // Mbit/s
    NS_LOG_UNCOND("Time: " << currentTime.GetSeconds() << "s, Throughput: " << throughput << " Mbit/s");
    lastTime = currentTime;
    Simulator::Schedule(MilliSeconds(100), &CalculateThroughput);
}


int main(int argc, char* argv[]) {
    bool tracing = false;
    std::string tcpCongestionControl = "TcpNewReno";
    std::string appDataRate = "50Mbps";
    uint32_t payloadSize = 1448;
    std::string phyRate = "HtMcs7";
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable PCAP tracing", tracing);
    cmd.AddValue("tcpCongestionControl", "TCP Congestion Control Algorithm", tcpCongestionControl);
    cmd.AddValue("appDataRate", "Application Data Rate", appDataRate);
    cmd.AddValue("payloadSize", "Payload size", payloadSize);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);

    cmd.Parse(argc, argv);

    // Configure TCP congestion control algorithm
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpCongestionControl));

    // Create nodes: one AP and one STA
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNode;
    staNode.Create(1);

    // Configure Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    // Configure Wi-Fi MAC for AP
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid),
        "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Configure Wi-Fi MAC for STA
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    // Set physical layer rate
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/HtConfiguration/ShortGuardIntervalSupported", BooleanValue(true));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
        "DataMode", StringValue(phyRate),
        "ControlMode", StringValue(phyRate));

    // Configure mobility models
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNode);
    
    // Enable MPDU aggregation
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/MpdusForAggregate", UintegerValue(64));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Mac/MaxAmsduSize", UintegerValue(7935) );

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create TCP application: STA (client) sends data to AP (server)
    uint16_t port = 5000;
    Address serverAddress(InetSocketAddress(apInterface.GetAddress(0), port));

    BulkSendHelper source("ns3::TcpSocketFactory", serverAddress);
    source.SetAttribute("MaxBytes", UintegerValue(0));
    source.SetAttribute("SendSize", UintegerValue(payloadSize));

    ApplicationContainer sourceApps = source.Install(staNode);
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(apNode);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));
    
    // Enable PCAP tracing
    if (tracing) {
        phy.EnablePcapAll("wifi-tcp-aggregation");
    }

    // Install FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
    Simulator::Schedule(Seconds(1.1), &CalculateThroughput);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND("Tx Packets = " << i->second.txPackets);
        NS_LOG_UNCOND("Rx Packets = " << i->second.rxPackets);
        NS_LOG_UNCOND("Lost Packets = " << i->second.lostPackets);
        NS_LOG_UNCOND("Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps");
    }

    Simulator::Destroy();

    return 0;
}