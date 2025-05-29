#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBackwardCompatibility");

// Function to convert Wi-Fi version string to standard and band
std::pair<WifiStandard, WifiPhyBand>
GetWifiStandardAndBand(const std::string& wifiVersion) {
    if (wifiVersion == "80211a") {
        return std::make_pair(WIFI_PHY_STANDARD_80211a, WIFI_PHY_BAND_5GHZ);
    } else if (wifiVersion == "80211b") {
        return std::make_pair(WIFI_PHY_STANDARD_80211b, WIFI_PHY_BAND_2GHZ);
    } else if (wifiVersion == "80211g") {
        return std::make_pair(WIFI_PHY_STANDARD_80211g, WIFI_PHY_BAND_2GHZ);
    } else if (wifiVersion == "80211n") {
        return std::make_pair(WIFI_PHY_STANDARD_80211n, WIFI_PHY_BAND_2GHZ);
    } else if (wifiVersion == "80211ac") {
        return std::make_pair(WIFI_PHY_STANDARD_80211ac, WIFI_PHY_BAND_5GHZ);
    } else if (wifiVersion == "80211ax") {
        return std::make_pair(WIFI_PHY_STANDARD_80211ax, WIFI_PHY_BAND_5GHZ);
    } else {
        return std::make_pair(WIFI_PHY_STANDARD_80211a, WIFI_PHY_BAND_5GHZ); // Default
    }
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    std::string wifiApStandard = "80211ac";
    std::string wifiStaStandard = "80211n";
    std::string rateAdaptationAlgorithm = "AarfWifiManager";
    std::string trafficSource = "sta"; // "ap", "sta", or "both"
    uint32_t packetSize = 1472;
    uint32_t numPackets = 1000;
    std::string dataRate = "54Mbps";
    double simulationTime = 10.0;
    double distance = 10.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("wifiApStandard", "Wifi standard for AP (80211a, 80211b, 80211g, 80211n, 80211ac, 80211ax)", wifiApStandard);
    cmd.AddValue("wifiStaStandard", "Wifi standard for STA (80211a, 80211b, 80211g, 80211n, 80211ac, 80211ax)", wifiStaStandard);
    cmd.AddValue("rateAdaptationAlgorithm", "Rate adaptation algorithm", rateAdaptationAlgorithm);
    cmd.AddValue("trafficSource", "Source of traffic (ap, sta, both)", trafficSource);
    cmd.AddValue("packetSize", "Size of packets", packetSize);
    cmd.AddValue("numPackets", "Number of packets", numPackets);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between AP and STA", distance);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiBackwardCompatibility", LOG_LEVEL_INFO);
    }

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);

    // Set up channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure AP
    WifiHelper wifiAp;
    wifiAp.SetStandard(GetWifiStandardAndBand(wifiApStandard).first);
    wifiAp.SetRemoteStationManager(rateAdaptationAlgorithm, "DataMode", StringValue(dataRate));

    NqosWifiMacHelper macAp = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns-3-ssid");
    macAp.SetType("ns3::ApWifiMac",
                   "Ssid", SsidValue(ssid),
                   "BeaconGeneration", BooleanValue(true),
                   "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifiAp.Install(phy, macAp, apNode);

    // Configure STA
    WifiHelper wifiSta;
    wifiSta.SetStandard(GetWifiStandardAndBand(wifiStaStandard).first);
    wifiSta.SetRemoteStationManager(rateAdaptationAlgorithm, "DataMode", StringValue(dataRate));

    NqosWifiMacHelper macSta = NqosWifiMacHelper::Default();
    macSta.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifiSta.Install(phy, macSta, staNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // UDP client and server
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp;
    if (trafficSource == "ap") {
        client.SetRemote(staInterface.GetAddress(0), port);
        clientApp = client.Install(apNode.Get(0));
    } else if (trafficSource == "sta") {
        client.SetRemote(apInterface.GetAddress(0), port);
        clientApp = client.Install(staNodes.Get(0));
    } else if (trafficSource == "both") {
        UdpClientHelper clientToAp(apInterface.GetAddress(0), port);
        clientToAp.SetAttribute("MaxPackets", UintegerValue(numPackets));
        clientToAp.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        clientToAp.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApp = clientToAp.Install(staNodes.Get(0));

        UdpClientHelper clientToSta(staInterface.GetAddress(0), port);
        clientToSta.SetAttribute("MaxPackets", UintegerValue(numPackets));
        clientToSta.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        clientToSta.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientAppAp = clientToSta.Install(apNode.Get(0));
        clientApp.Add(clientAppAp);
    } else {
        std::cerr << "Invalid traffic source specified." << std::endl;
        return 1;
    }

    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalRxBytes = 0;
    double totalTime = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        totalRxBytes += i->second.rxBytes;
        totalTime = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
    }

    double overallThroughput = totalRxBytes * 8.0 / totalTime / 1000000;
    std::cout << "Overall Throughput: " << overallThroughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}