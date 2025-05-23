#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line-module.h"

#include <iostream>
#include <string>

using namespace ns3;

// Global variables for throughput calculation
static uint64_t g_totalBytesReceived = 0;
static Ptr<PacketSink> g_sinkApp = nullptr;

void CalcThroughput(Time interval)
{
    if (g_sinkApp == nullptr)
    {
        return; 
    }

    uint64_t currentBytes = g_sinkApp->GetTotalRx();
    double currentThroughputMbps = (double)((currentBytes - g_totalBytesReceived) * 8.0) / (interval.GetSeconds() * 1000000.0);
    
    std::cout << Simulator::Now().GetSeconds() << "\t" << currentThroughputMbps << std::endl;
    
    g_totalBytesReceived = currentBytes;

    Simulator::Schedule(interval, &CalcThroughput, interval);
}

int main(int argc, char* argv[])
{
    // Default values for parameters
    std::string tcpAlg = "ns3::TcpNewReno";
    double appDataRateMb = 50.0; // Mbps
    uint32_t payloadSize = 1472; // Bytes
    std::string phyRate = "HtMcs7"; // 802.11n MCS Index 7
    double simDuration = 10.0; // Seconds
    bool enablePcap = false;

    // Command line parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("tcp", "TCP congestion control algorithm (e.g., TcpNewReno, TcpCubic)", tcpAlg);
    cmd.AddValue("appRate", "Application data rate in Mbps", appDataRateMb);
    cmd.AddValue("payloadSize", "Application payload size in bytes", payloadSize);
    cmd.AddValue("phyRate", "Physical layer bitrate (e.g., HtMcs7, HtMcs0 for 802.11n)", phyRate);
    cmd.AddValue("duration", "Simulation duration in seconds", simDuration);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Set TCP congestion control algorithm
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpAlg));
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketBase::InitialCwnd", UintegerValue(10)); // 10 segments

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(2); // Node 0: AP, Node 1: STA
    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); 
    
    // Use ConstantRateWifiManager to set a fixed physical layer rate (MCS)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyRate));

    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel");
    channel.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20)); // dBm
    phy.Set("TxPowerEnd", DoubleValue(20));   // dBm
    phy.Set("RxSensitivity", DoubleValue(-90)); // dBm
    phy.Set("ChannelWidth", UintegerValue(40)); // 40 MHz channel width for 802.11n

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default(); 
    Ssid ssid = Ssid("tcp-80211n-test");

    // Configure AP
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Configure STA
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set fixed positions for AP and STA
    apNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0)); // STA 10m away

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevice);
    address.Assign(staDevice);

    Ipv4Address apIp = interfaces.GetAddress(0);

    // Applications (TCP BulkSend and PacketSink)
    uint16_t port = 9; // Discard port

    // PacketSink on AP
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(apNode);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simDuration));
    g_sinkApp = DynamicCast<PacketSink>(sinkApps.Get(0)); // Store pointer for throughput calculation

    // BulkSend on STA
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(apIp, port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely until stop time
    bulkSendHelper.SetAttribute("SendRate", DataRateValue(DataRate(appDataRateMb * 1000000))); // Convert Mbps to bps
    bulkSendHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer sourceApps = bulkSendHelper.Install(staNode);
    sourceApps.Start(Seconds(1.0)); // Start sending 1 second into simulation
    sourceApps.Stop(Seconds(simDuration));

    // PCAP tracing
    if (enablePcap)
    {
        phy.EnablePcap("tcp-80211n-ap", apDevice.Get(0));
        phy.EnablePcap("tcp-80211n-sta", staDevice.Get(0));
    }
    
    // Schedule throughput calculation
    Time throughputInterval = MilliSeconds(100);
    std::cout << "Time (s)\tThroughput (Mbps)" << std::endl;
    Simulator::Schedule(throughputInterval, &CalcThroughput, throughputInterval);

    // Run simulation
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    // Clean up
    Simulator::Destroy();

    return 0;
}