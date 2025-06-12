#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBackwardCompatibility");

// Function to convert Wi-Fi version string to standard and band
std::pair<WifiStandard, WifiBand> GetWifiStandardAndBand(const std::string& wifiVersion) {
    if (wifiVersion == "80211a") {
        return std::make_pair(WIFI_PHY_STANDARD_80211a, WIFI_BAND_5GHZ);
    } else if (wifiVersion == "80211b") {
        return std::make_pair(WIFI_PHY_STANDARD_80211b, WIFI_BAND_2GHZ);
    } else if (wifiVersion == "80211g") {
        return std::make_pair(WIFI_PHY_STANDARD_80211g, WIFI_BAND_2GHZ);
    } else if (wifiVersion == "80211n") {
        return std::make_pair(WIFI_PHY_STANDARD_80211n, WIFI_BAND_2GHZ); // Can be 5GHz as well
    } else if (wifiVersion == "80211ac") {
        return std::make_pair(WIFI_PHY_STANDARD_80211ac, WIFI_BAND_5GHZ);
    } else if (wifiVersion == "80211ax") {
        return std::make_pair(WIFI_PHY_STANDARD_80211ax, WIFI_BAND_5GHZ); // Can be 2.4GHz as well
    } else {
        // Default to 802.11a
        std::cout << "Unknown Wi-Fi version: " << wifiVersion << ". Defaulting to 802.11a." << std::endl;
        return std::make_pair(WIFI_PHY_STANDARD_80211a, WIFI_BAND_5GHZ);
    }
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string wifiApVersion = "80211ac";
    std::string wifiStaVersion = "80211n";
    std::string rateAdaptationAlgorithm = "AarfWifiManager"; // Example: "AarfWifiManager", "IdealWifiManager"
    bool enableTrafficFromAp = true;
    bool enableTrafficFromSta = true;
    double simulationTime = 10.0; //seconds
    std::string phyMode;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("wifiApVersion", "Wifi standard for the Access Point (e.g., 80211a, 80211n, 80211ac)", wifiApVersion);
    cmd.AddValue("wifiStaVersion", "Wifi standard for the Station (e.g., 80211a, 80211n, 80211ac)", wifiStaVersion);
    cmd.AddValue("rateAdaptationAlgorithm", "Rate Adaptation Algorithm", rateAdaptationAlgorithm);
    cmd.AddValue("enableTrafficFromAp", "Enable traffic generation from Access Point", enableTrafficFromAp);
    cmd.AddValue("enableTrafficFromSta", "Enable traffic generation from Station", enableTrafficFromSta);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiBackwardCompatibility", LOG_LEVEL_INFO);
    }

    // Set up simulation parameters
    std::pair<WifiStandard, WifiBand> apWifiInfo = GetWifiStandardAndBand(wifiApVersion);
    WifiStandard apWifiStandard = apWifiInfo.first;
    WifiBand apWifiBand = apWifiInfo.second;

    std::pair<WifiStandard, WifiBand> staWifiInfo = GetWifiStandardAndBand(wifiStaVersion);
    WifiStandard staWifiStandard = staWifiInfo.first;
    WifiBand staWifiBand = staWifiInfo.second;

    // Create nodes: AP and STA
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNode;
    staNode.Create(1);

    // Configure the PHY and MAC layers
    WifiHelper wifi;
    wifi.SetStandard(apWifiStandard); // Use same standard for both AP and STA initially
    phyMode = wifiApVersion;
    if (staWifiVersion == "80211n") {
        phyMode = "Ht40";
    }
    else if (staWifiVersion == "80211ac") {
        phyMode = "Vht80";
    }
    else if (staWifiVersion == "80211ax") {
        phyMode = "He80";
    }
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Simple();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(20));
    wifiPhy.Set("TxPowerEnd", DoubleValue(20));
    wifiPhy.Set("TxPowerLevels", UintegerValue(1));
    wifiPhy.Set("RxNoiseFigure", DoubleValue(7));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-69));
    wifiPhy.Set("CcaEdThreshold", DoubleValue(-66));

    WifiMacHelper wifiMac;

    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::ApWifiMac",
                     "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid),
                     "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, staNode);

    // Mobility
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
    mobility.Install(staNode);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // Set up UDP server on STA
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(staNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    // Set up UDP client on AP (or STA, or both)
    uint16_t port = 9;
    Address serverAddress = InetSocketAddress(staInterface.GetAddress(0), port);
    UdpClientHelper client(serverAddress);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time("1ms")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;

    if (enableTrafficFromAp) {
        clientApps.Add(client.Install(apNode.Get(0)));
    }
    if (enableTrafficFromSta) {
        UdpClientHelper clientSta(serverAddress);
        clientSta.SetAttribute("MaxPackets", UintegerValue(4294967295));
        clientSta.SetAttribute("Interval", TimeValue(Time("1ms")));
        clientSta.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(clientSta.Install(staNode.Get(0)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalRxBytes = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Flow Duration:" << i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        totalRxBytes += i->second.rxBytes;
    }

    Simulator::Destroy();

    return 0;
}