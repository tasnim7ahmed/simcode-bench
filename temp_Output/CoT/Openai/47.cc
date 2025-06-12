#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAxSpatialReuseExample");

void LogSRReset(std::string context, uint32_t count)
{
    static std::ofstream logFile("spatial_reuse_reset.log", std::ios_base::app);
    logFile << Simulator::Now().GetSeconds() << " " << context << " reset count: " << count << std::endl;
}

int main(int argc, char *argv[])
{
    // Default parameters
    double d1 = 5.0;   // Distance Between STA1 and AP1 (meters)
    double d2 = 5.0;   // Distance Between STA2 and AP2 (meters)
    double d3 = 20.0;  // Distance Between (AP1-AP2) (meters)
    double txPower = 16.0; // Transmit power in dBm
    double ccaEdThreshold = -62.0; // dBm
    double obssPdThreshold = -72.0; // dBm
    bool enableObssPd = true;
    uint32_t channelWidth = 20; // MHz
    double simulationTime = 20.0; // seconds
    std::string dataRate = "100Mbps";
    Time interPacketInterval = MilliSeconds(1);
    uint32_t packetSize = 1024; // bytes

    // Command Line Arguments
    CommandLine cmd;
    cmd.AddValue("d1", "Distance STA1-AP1 (meters)", d1);
    cmd.AddValue("d2", "Distance STA2-AP2 (meters)", d2);
    cmd.AddValue("d3", "Distance AP1-AP2 (meters)", d3);
    cmd.AddValue("txPower", "Tx Power (dBm)", txPower);
    cmd.AddValue("ccaEdThreshold", "CCA-ED threshold (dBm)", ccaEdThreshold);
    cmd.AddValue("obssPdThreshold", "OBSS-PD threshold (dBm)", obssPdThreshold);
    cmd.AddValue("enableObssPd", "Enable OBSS-PD Spatial Reuse", enableObssPd);
    cmd.AddValue("channelWidth", "WiFi Channel Width (20,40,80)", channelWidth);
    cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);
    cmd.AddValue("dataRate", "Offered traffic data rate", dataRate);
    cmd.AddValue("interval", "Inter-packet interval (ms)", interPacketInterval);
    cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
    cmd.Parse(argc, argv);

    // Create nodes: 2 APs, 2 STAs
    NodeContainer wifiApNodes, wifiStaNodes;
    wifiApNodes.Create(2);
    wifiStaNodes.Create(2);

    // Set positions: AP1 at (0,0), AP2 at (d3, 0), STA1 at (0,d1), STA2 at (d3, d2)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));           // AP1
    positionAlloc->Add(Vector(d3, 0.0, 0.0));            // AP2
    positionAlloc->Add(Vector(0.0, d1, 0.0));            // STA1
    positionAlloc->Add(Vector(d3, d2, 0.0));             // STA2
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);
    mobility.Install(wifiStaNodes);

    // Configure Wi-Fi (802.11ax)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);

    WifiMacHelper mac;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(txPower));
    phy.Set("TxPowerEnd", DoubleValue(txPower));
    phy.Set("CcaEdThreshold", DoubleValue(ccaEdThreshold));
    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    std::string channelWidthStr = std::to_string(channelWidth);

    // Set HE (802.11ax) parameters and spatial reuse
    HeWifiMacHelper heMac;
    if (enableObssPd)
    {
        phy.Set("EnableObssPd", BooleanValue(true));
        phy.Set("ObssPdThreshold", DoubleValue(obssPdThreshold));
        Config::SetDefault("ns3::HeConfiguration::SpatialReuseSupported", BooleanValue(true));
    }
    else
    {
        phy.Set("EnableObssPd", BooleanValue(false));
        Config::SetDefault("ns3::HeConfiguration::SpatialReuseSupported", BooleanValue(false));
    }

    // Install devices - BSS 1
    Ssid ssid1 = Ssid("bss-1");
    // AP1
    NetDeviceContainer apDevice1;
    heMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevice1 = wifi.Install(phy, heMac, wifiApNodes.Get(0));
    // STA1
    NetDeviceContainer staDevice1;
    heMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevice1 = wifi.Install(phy, heMac, wifiStaNodes.Get(0));

    // BSS 2
    Ssid ssid2 = Ssid("bss-2");
    // AP2
    NetDeviceContainer apDevice2;
    heMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevice2 = wifi.Install(phy, heMac, wifiApNodes.Get(1));
    // STA2
    NetDeviceContainer staDevice2;
    heMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevice2 = wifi.Install(phy, heMac, wifiStaNodes.Get(1));

    // Logging spatial reuse events (simulate via PhyReset trace)
    Config::ConnectWithoutContext("/NodeList/0/DeviceList/*/Phy/State/Reset", MakeBoundCallback(&LogSRReset, "AP1"));
    Config::ConnectWithoutContext("/NodeList/1/DeviceList/*/Phy/State/Reset", MakeBoundCallback(&LogSRReset, "AP2"));
    Config::ConnectWithoutContext("/NodeList/2/DeviceList/*/Phy/State/Reset", MakeBoundCallback(&LogSRReset, "STA1"));
    Config::ConnectWithoutContext("/NodeList/3/DeviceList/*/Phy/State/Reset", MakeBoundCallback(&LogSRReset, "STA2"));

    // Internet stack and IP
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIf1 = address.Assign(apDevice1);
    Ipv4InterfaceContainer staIf1 = address.Assign(staDevice1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apIf2 = address.Assign(apDevice2);
    Ipv4InterfaceContainer staIf2 = address.Assign(staDevice2);

    // Setup UDP traffic: STA1->AP1, STA2->AP2
    uint16_t port1 = 9001;
    uint16_t port2 = 9002;

    // AP1: sink
    UdpServerHelper server1(port1);
    ApplicationContainer serverApps1 = server1.Install(wifiApNodes.Get(0));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(simulationTime + 1));

    // STA1: sender
    UdpClientHelper client1(apIf1.GetAddress(0), port1);
    client1.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client1.SetAttribute("Interval", TimeValue(interPacketInterval));
    client1.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps1 = client1.Install(wifiStaNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simulationTime + 1));

    // AP2: sink
    UdpServerHelper server2(port2);
    ApplicationContainer serverApps2 = server2.Install(wifiApNodes.Get(1));
    serverApps2.Start(Seconds(0.0));
    serverApps2.Stop(Seconds(simulationTime + 1));

    // STA2: sender
    UdpClientHelper client2(apIf2.GetAddress(0), port2);
    client2.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client2.SetAttribute("Interval", TimeValue(interPacketInterval));
    client2.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps2 = client2.Install(wifiStaNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(simulationTime + 1));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 2));
    Simulator::Run();

    // Throughput Calculation
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double throughput1 = 0.0, throughput2 = 0.0;
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == apIf1.GetAddress(0))
        {
            throughput1 += (flow.second.rxBytes * 8.0) / (simulationTime * 1e6); // Mbps
        }
        if (t.destinationAddress == apIf2.GetAddress(0))
        {
            throughput2 += (flow.second.rxBytes * 8.0) / (simulationTime * 1e6); // Mbps
        }
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Simulation Results:" << std::endl;
    std::cout << "OBSS-PD spatial reuse: " << (enableObssPd ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Channel width: " << channelWidth << " MHz" << std::endl;
    std::cout << "Throughput (BSS1, STA1->AP1): " << throughput1 << " Mbps" << std::endl;
    std::cout << "Throughput (BSS2, STA2->AP2): " << throughput2 << " Mbps" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    Simulator::Destroy();
    return 0;
}