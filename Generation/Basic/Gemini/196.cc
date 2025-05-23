#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

// NS_LOG_COMPONENT_DEFINE("WifiUdpSimulation"); // This line is not needed for the final output

// Global counters for transmitted packets per station
uint64_t g_sta1TxPackets = 0;
uint64_t g_sta2TxPackets = 0;

/**
 * \brief Callback for STA1's device Tx trace source.
 * Increments a global counter for STA1's transmitted packets.
 * \param p The packet being transmitted.
 */
void Sta1TxTrace(Ptr<const Packet> p)
{
    g_sta1TxPackets++;
}

/**
 * \brief Callback for STA2's device Tx trace source.
 * Increments a global counter for STA2's transmitted packets.
 * \param p The packet being transmitted.
 */
void Sta2TxTrace(Ptr<const Packet> p)
{
    g_sta2TxPackets++;
}

int main(int argc, char* argv[])
{
    // Set default attributes for WiFi
    // Command line arguments for customisation (optional)
    // CommandLine cmd(__FILE__);
    // cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    // cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
    // cmd.AddValue("dataRate", "Application data rate", dataRate);
    // cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    // cmd.Parse(argc, argv);

    // Set a fixed random seed for reproducibility
    RngSeedManager::SetSeed(1);
    RngStream::Set  StreamNumber(1);

    // Configure WiFi standard (e.g., 802.11n in 5GHz band)
    WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n_5GHZ);

    // Create channel and PHY
    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO); // Needed for correct pcap format

    // Create nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    // Configure Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Position AP at origin
    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    // Position STAs
    Ptr<ConstantPositionMobilityModel> sta1Mobility = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    sta1Mobility->SetPosition(Vector(10.0, 0.0, 0.0)); // STA1 10m from AP on X-axis

    Ptr<ConstantPositionMobilityModel> sta2Mobility = staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    sta2Mobility->SetPosition(Vector(0.0, 10.0, 0.0)); // STA2 10m from AP on Y-axis

    // Configure MAC for AP
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Configure MAC for STAs
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);

    // Configure UDP traffic (OnOff applications from STAs to AP)
    // Use different ports for each STA's traffic so PacketSink can differentiate
    uint16_t port1 = 9;  // Port for STA1 to AP traffic
    uint16_t port2 = 10; // Port for STA2 to AP traffic

    // PacketSink for STA1's traffic on AP
    PacketSinkHelper sinkHelper1("ns3::UdpSocketFactory",
                                 InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApps1 = sinkHelper1.Install(apNode.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(10.0));

    // PacketSink for STA2's traffic on AP
    PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory",
                                 InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApps2 = sinkHelper2.Install(apNode.Get(0));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(10.0));

    // OnOff application for STA1
    OnOffHelper onoff1("ns3::UdpSocketFactory", Address());
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    onoff1.SetAttribute("DataRate", StringValue("1Mbps"));   // 1 Mbps data rate
    AddressValue remoteAddress1(InetSocketAddress(apInterfaces.GetAddress(0), port1));
    onoff1.SetAttribute("Remote", remoteAddress1);
    ApplicationContainer clientApps1 = onoff1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(1.0)); // Start after 1 second
    clientApps1.Stop(Seconds(9.0));  // Stop at 9 seconds (8 seconds of data transfer)

    // OnOff application for STA2
    OnOffHelper onoff2("ns3::UdpSocketFactory", Address());
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));
    onoff2.SetAttribute("DataRate", StringValue("1Mbps"));
    AddressValue remoteAddress2(InetSocketAddress(apInterfaces.GetAddress(0), port2));
    onoff2.SetAttribute("Remote", remoteAddress2);
    ApplicationContainer clientApps2 = onoff2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(9.0));

    // Enable pcap tracing
    phy.EnablePcapAll("wifi-udp-simulation");

    // Connect Tx trace sources to count packets transmitted by each STA's WiFi device
    staDevices.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&Sta1TxTrace));
    staDevices.Get(1)->TraceConnectWithoutContext("Tx", MakeCallback(&Sta2TxTrace));

    // Optional: NetAnim tracing
    AnimationInterface anim("wifi-udp-simulation.xml");
    anim.SetConstantPosition(apNode.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(staNodes.Get(0), 10.0, 0.0);
    anim.SetConstantPosition(staNodes.Get(1), 0.0, 10.0);

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();

    // Retrieve and print results
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps1.Get(0));
    Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(sinkApps2.Get(0));

    double simulationTime = Simulator::Now().GetSeconds(); // Actual simulation end time
    double applicationDuration = 8.0; // 9.0 - 1.0 = 8.0 seconds of application activity

    // STA 1 Results
    std::cout << "--- STA 1 Results ---" << std::endl;
    std::cout << "  Packets Transmitted (MAC-level): " << g_sta1TxPackets << std::endl;
    uint64_t totalRxBytes1 = sink1->GetTotalRx();
    std::cout << "  Total Received Bytes (at AP from STA1): " << totalRxBytes1 << std::endl;
    double throughput1 = (double)totalRxBytes1 * 8 / (applicationDuration * 1000000.0); // Mbps
    std::cout << "  Throughput (STA1 to AP): " << throughput1 << " Mbps" << std::endl;
    std::cout << "  Received Packets (at AP from STA1): " << sink1->GetTotalRxPackets() << std::endl;

    // STA 2 Results
    std::cout << "--- STA 2 Results ---" << std::endl;
    std::cout << "  Packets Transmitted (MAC-level): " << g_sta2TxPackets << std::endl;
    uint64_t totalRxBytes2 = sink2->GetTotalRx();
    std::cout << "  Total Received Bytes (at AP from STA2): " << totalRxBytes2 << std::endl;
    double throughput2 = (double)totalRxBytes2 * 8 / (applicationDuration * 1000000.0); // Mbps
    std::cout << "  Throughput (STA2 to AP): " << throughput2 << " Mbps" << std::endl;
    std::cout << "  Received Packets (at AP from STA2): " << sink2->GetTotalRxPackets() << std::endl;

    // Clean up
    Simulator::Destroy();

    return 0;
}