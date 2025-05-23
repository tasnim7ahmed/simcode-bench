#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-global-routing-helper.h"

// Do not use using namespace ns3; to avoid potential conflicts with global scope
// Instead, use ns3:: prefix for ns-3 specific types and functions.

// Global pointer to sink application to get Rx bytes
ns3::Ptr<ns3::PacketSink> g_sinkApp;

/**
 * \brief Calculates and prints the average throughput.
 * \param totalTime The total simulation time over which throughput is calculated.
 */
void CalculateThroughput(double totalTime)
{
    if (!g_sinkApp)
    {
        ns3::NS_LOG_ERROR("PacketSink application pointer is null.");
        return;
    }

    double totalBytesRx = g_sinkApp->GetTotalRx();
    // Convert bytes to Mbits and time to seconds for Mbps
    double throughputMbps = (totalBytesRx * 8.0) / (totalTime * 1000000.0);
    ns3::NS_LOG_UNCOND("Average throughput: " << throughputMbps << " Mbps");
}

int main(int argc, char *argv[])
{
    // 1. Configuration Variables and Command Line Parsing
    double simulationTime = 10.0; // seconds
    std::string p2pDataRate = "100Mbps";
    std::string p2pDelay = "2ms";
    std::string wifiPhyMode = "HtMcs7"; // 802.11n MCS 7
    std::string tcpVariant = "ns3::TcpNewReno";

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("p2pDataRate", "Data rate for point-to-point links", p2pDataRate);
    cmd.AddValue("p2pDelay", "Delay for point-to-point links", p2pDelay);
    cmd.AddValue("wifiPhyMode", "Phy mode for Wi-Fi", wifiPhyMode);
    cmd.AddValue("tcpVariant", "TCP variant to use", tcpVariant);
    cmd.Parse(argc, argv);

    // Set TCP variant
    ns3::TypeId tid = ns3::TypeId::LookupByName(tcpVariant);
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::PointerValue(tid));
    // Increase TCP send/receive buffer sizes for better performance
    ns3::Config::SetDefault("ns3::TcpSocket::SndBufSize", ns3::UintegerValue(1 << 20)); // 1MB
    ns3::Config::SetDefault("ns3::TcpSocket::RcvBufSize", ns3::UintegerValue(1 << 20)); // 1MB

    // 2. Create Nodes
    ns3::NodeContainer wiredNodes;
    wiredNodes.Create(4); // n0, n1 (gateway), n2, n3

    ns3::NodeContainer wirelessStaNodes;
    wirelessStaNodes.Create(2); // sta0, sta1 (total 3 wireless nodes including AP)

    // The gateway node (n1 from wiredNodes) will also host the Wi-Fi AP
    ns3::Ptr<ns3::Node> gatewayNode = wiredNodes.Get(1);

    // 3. Wired Network (Point-to-Point Chain)
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue(p2pDataRate));
    p2p.SetChannelAttribute("Delay", ns3::StringValue(p2pDelay));

    ns3::NetDeviceContainer p2pDevices01, p2pDevices12, p2pDevices23;
    ns3::Ipv4InterfaceContainer ipInterfaces01, ipInterfaces12, ipInterfaces23;

    ns3::Ipv4AddressHelper ipv4;
    ns3::InternetStackHelper internet;
    internet.Install(wiredNodes);

    // Link n0 <-> n1 (gateway)
    p2pDevices01 = p2p.Install(wiredNodes.Get(0), wiredNodes.Get(1));
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipInterfaces01 = ipv4.Assign(p2pDevices01);

    // Link n1 (gateway) <-> n2
    p2pDevices12 = p2p.Install(wiredNodes.Get(1), wiredNodes.Get(2));
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipInterfaces12 = ipv4.Assign(p2pDevices12);

    // Link n2 <-> n3 (sink node)
    p2pDevices23 = p2p.Install(wiredNodes.Get(2), wiredNodes.Get(3));
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ipInterfaces23 = ipv4.Assign(p2pDevices23);

    // 4. Wireless Network (Wi-Fi)
    ns3::InternetStackHelper internetWifi;
    internetWifi.Install(wirelessStaNodes); // Install InternetStack on STA nodes

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n);
    // Use HT Wifi Manager for 802.11n
    wifi.SetRemoteStationManager("ns3::HtWifiManager", "ActiveProbing", ns3::BooleanValue(false));

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    channel.SetPcapDataAttributes("ns3::PcapHelperForDevice::SupportedRadiotapFields", ns3::UintegerValue(
                                      (1 << 2)  | // TX flags
                                      (1 << 5)  | // Data Rate
                                      (1 << 8)  | // Signal strength
                                      (1 << 9)  | // Noise strength
                                      (1 << 10) | // channel
                                      (1 << 11) | // freq
                                      (1 << 13) | // antenna
                                      (1 << 14) | // rx_flags
                                      (1 << 19)   // VHT_Mcs_Nss
                                  ));

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", ns3::DoubleValue(20.0)); // dBm
    phy.Set("TxPowerEnd", ns3::DoubleValue(20.0));   // dBm
    phy.Set("TxPowerLevels", ns3::UintegerValue(1)); // One power level
    phy.Set("TxGain", ns3::DoubleValue(0));          // dB
    phy.Set("RxGain", ns3::DoubleValue(0));          // dB
    phy.Set("RxSensitivity", ns3::DoubleValue(-90.0)); // dBm
    phy.Set("CcaMode1Threshold", ns3::DoubleValue(-80.0)); // dBm

    // Configure MAC for AP and STA
    ns3::HtWifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("my-wifi-ssid");

    // AP (gatewayNode)
    ns3::NetDeviceContainer apDevices;
    mac.SetType("ns3::HtWifiApMac",
                "Ssid", ns3::SsidValue(ssid),
                "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400)), // 102.4ms default
                "EnableBeaconJitter", ns3::BooleanValue(false));
    apDevices = wifi.Install(phy, mac, gatewayNode);

    // STAs (wirelessStaNodes)
    ns3::NetDeviceContainer staDevices;
    mac.SetType("ns3::HtWifiStaMac",
                "Ssid", ns3::SsidValue(ssid),
                "ActiveProbing", ns3::BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wirelessStaNodes);

    // Mobility for all nodes (static for now)
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(5.0),
                                  "DeltaY", ns3::DoubleValue(5.0),
                                  "GridWidth", ns3::UintegerValue(3),
                                  "LayoutType", ns3::StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wiredNodes); // Install on all wired nodes
    mobility.Install(wirelessStaNodes); // Install on wireless STA nodes

    // Assign IP addresses to Wi-Fi devices
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer ipApInterface = ipv4.Assign(apDevices);
    ns3::Ipv4InterfaceContainer ipStaInterfaces = ipv4.Assign(staDevices);

    // 5. IP Routing - Enable global routing
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. TCP Traffic Generation
    // Sink on n3 (wiredNodes.Get(3))
    ns3::Ptr<ns3::Node> sinkNode = wiredNodes.Get(3);
    uint16_t sinkPort = 9;
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort));
    ns3::ApplicationContainer sinkApp = sinkHelper.Install(sinkNode);
    g_sinkApp = ns3::DynamicCast<ns3::PacketSink>(sinkApp.Get(0)); // Store for throughput calculation

    sinkApp.Start(ns3::Seconds(0.0));
    sinkApp.Stop(ns3::Seconds(simulationTime + 1.0)); // Stop slightly after sim end to ensure all packets are processed

    // OnOff applications on wirelessStaNodes (sta0, sta1)
    // Destination is n3's IP address on the 10.1.3.x network
    ns3::OnOffHelper onoff("ns3::TcpSocketFactory",
                          ns3::Address(ns3::InetSocketAddress(ipInterfaces23.GetAddress(1), sinkPort)));

    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(1460)); // Standard MTU minus headers
    onoff.SetAttribute("DataRate", ns3::StringValue("10Mbps")); // Each STA sends 10 Mbps

    ns3::ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wirelessStaNodes.GetN(); ++i)
    {
        clientApps.Add(onoff.Install(wirelessStaNodes.Get(i)));
    }
    clientApps.Start(ns3::Seconds(1.0)); // Start client apps after 1 second
    clientApps.Stop(ns3::Seconds(simulationTime));

    // 7. Pcap Tracing
    p2p.EnablePcapAll("hybrid-network-p2p");
    phy.EnablePcapAll("hybrid-network-wifi");

    // 8. Simulation Run
    ns3::Simulator::Stop(ns3::Seconds(simulationTime + 0.1)); // Stop just after applications finish
    ns3::Simulator::Run();
    
    // 9. Throughput Calculation
    CalculateThroughput(simulationTime);

    ns3::Simulator::Destroy();
    return 0;
}