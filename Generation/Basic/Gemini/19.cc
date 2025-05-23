#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <string>
#include <utility> // For std::pair
#include <map>     // For std::map used by FlowMonitor

// Use NS_LOG for debugging if needed, but not for final output.
// NS_LOG_COMPONENT_DEFINE("WifiBackwardCompatibility");

using namespace ns3;

/**
 * @brief Converts a Wi-Fi version string to its corresponding WifiStandard and WifiPhyBand.
 *
 * This function maps common Wi-Fi standard strings (e.g., "80211a", "80211ac")
 * to their respective ns-3 WifiStandard enum values and a suitable WifiPhyBand.
 * For standards like 802.11n or 802.11ax that can operate in multiple bands,
 * a default band (e.g., 5GHz) is chosen for demonstration.
 *
 * @param version_str The string representation of the Wi-Fi standard.
 * @return A pair containing the WifiStandard and WifiPhyBand.
 * @throws Fatal error if an unknown version string is provided.
 */
std::pair<WifiStandard, WifiPhyBand> ConvertWifiVersion(const std::string& version_str) {
    WifiStandard standard;
    WifiPhyBand band;

    if (version_str == "80211a") {
        standard = WIFI_STANDARD_80211a;
        band = WIFI_PHY_BAND_5GHZ;
    } else if (version_str == "80211b") {
        standard = WIFI_STANDARD_80211b;
        band = WIFI_PHY_BAND_2_4GHZ;
    } else if (version_str == "80211g") {
        standard = WIFI_STANDARD_80211g;
        band = WIFI_PHY_BAND_2_4GHZ;
    } else if (version_str == "80211n") {
        standard = WIFI_STANDARD_80211n;
        // 802.11n can operate in 2.4 GHz or 5 GHz. Choosing 5GHz for this example.
        band = WIFI_PHY_BAND_5GHZ;
    } else if (version_str == "80211ac") {
        standard = WIFI_STANDARD_80211ac;
        band = WIFI_PHY_BAND_5GHZ;
    } else if (version_str == "80211ax") {
        standard = WIFI_STANDARD_80211ax;
        // 802.11ax can operate in 2.4 GHz, 5 GHz, or 6 GHz. Choosing 5GHz for this example.
        band = WIFI_PHY_BAND_5GHZ;
    } else {
        NS_FATAL_ERROR("Unknown Wi-Fi version string: " << version_str);
    }
    return {standard, band};
}

int main(int argc, char *argv[]) {
    // 1. Command line arguments to configure the simulation
    std::string apWifiVersion = "80211ax";       // Default AP Wi-Fi standard
    std::string staWifiVersion = "80211ac";      // Default STA Wi-Fi standard (backward compatible)
    std::string apRateManager = "ns3::AarfWifiManager"; // Rate adaptation for AP
    std::string staRateManager = "ns3::AarfWifiManager"; // Rate adaptation for STA
    uint32_t payloadSize = 1472;                 // UDP payload size in bytes (max for standard MTU)
    double simulationTime = 10.0;                // Total simulation time in seconds
    std::string trafficDirection = "ApToSta";    // "ApToSta", "StaToAp", "Both"
    std::string dataRateStr = "50Mbps";          // Target data rate for UDP applications

    CommandLine cmd(__FILE__);
    cmd.AddValue("apVersion", "Wi-Fi version for AP (e.g., 80211ax)", apWifiVersion);
    cmd.AddValue("staVersion", "Wi-Fi version for STA (e.g., 80211ac)", staWifiVersion);
    cmd.AddValue("apRateManager", "Rate adaptation algorithm for AP (e.g., ns3::AarfWifiManager)", apRateManager);
    cmd.AddValue("staRateManager", "Rate adaptation algorithm for STA (e.g., ns3::AarfWifiManager)", staRateManager);
    cmd.AddValue("payloadSize", "Payload size of UDP packets (bytes)", payloadSize);
    cmd.AddValue("simTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("traffic", "Traffic direction (ApToSta, StaToAp, Both)", trafficDirection);
    cmd.AddValue("dataRate", "Target data rate for UDP client (e.g., 50Mbps)", dataRateStr);
    cmd.Parse(argc, argv);

    DataRate dataRate(dataRateStr);
    
    // Validate traffic direction input
    if (trafficDirection != "ApToSta" && trafficDirection != "StaToAp" && trafficDirection != "Both") {
        NS_FATAL_ERROR("Invalid traffic direction: " << trafficDirection << ". Must be ApToSta, StaToAp, or Both.");
    }

    // 2. Node creation: one Access Point (AP) and one Station (STA)
    NodeContainer nodes;
    nodes.Create(2); 
    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // 3. Mobility setup
    MobilityHelper mobility;

    // AP is stationary at the origin
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    apNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // STA moves with a constant velocity
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(staNode);
    // STA starts at (5,0,0) and moves to (10,0,0) over the simulation time
    staNode->GetObject<ConstantVelocityMobilityModel>()->SetPosition(Vector(5.0, 0.0, 0.0));
    // Calculate velocity required to move 5m in 'simulationTime' seconds
    staNode->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(5.0 / simulationTime, 0.0, 0.0));

    // 4. Wi-Fi configuration: common helper instances
    WifiHelper wifi;
    WifiPhyHelper phy;
    phy.SetErrorRateModel("ns3::NistErrorRateModel"); // A common choice for generic simulations
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // 5. AP Wi-Fi configuration
    std::pair<WifiStandard, WifiPhyBand> apConfig = ConvertWifiVersion(apWifiVersion);
    wifi.SetStandard(apConfig.first);
    wifi.SetRemoteStationManager(apRateManager);
    // Channel settings: channel number (0), channel width (20 MHz), and physical band
    phy.Set("ChannelSettings", StringValue("{0, 20, " + std::to_string(static_cast<int>(apConfig.second)) + "}"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi"); // Common SSID for AP and STA
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400))); // Default beacon interval
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // 6. STA Wi-Fi configuration
    std::pair<WifiStandard, WifiPhyBand> staConfig = ConvertWifiVersion(staWifiVersion);
    wifi.SetStandard(staConfig.first);
    wifi.SetRemoteStationManager(staRateManager);
    // Channel settings: channel number (0), channel width (20 MHz), and physical band
    phy.Set("ChannelSettings", StringValue("{0, 20, " + std::to_string(static_cast<int>(staConfig.second)) + "}"));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false)); // STA passively scans for APs
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    // 7. Install Internet Stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 8. Populate global routing tables for IP communication
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 9. Setup UDP Client/Server Applications
    uint16_t port = 9; // Discard port
    // Calculate interval between packets to achieve the desired data rate
    // Interval = (PacketSize in bits) / (DataRate in bits/second)
    Time packetInterval = MicroSeconds(static_cast<uint64_t>(payloadSize * 8.0 / dataRate.GetBitsPerSecond() * 1e6));

    if (trafficDirection == "ApToSta" || trafficDirection == "Both") {
        // Server on STA (receives traffic)
        UdpEchoServerHelper staUdpServer(port);
        ApplicationContainer staServerApps = staUdpServer.Install(staNode);
        staServerApps.Start(Seconds(0.0));
        staServerApps.Stop(Seconds(simulationTime));

        // Client on AP (sends traffic to STA)
        UdpClientHelper apUdpClient(staInterfaces.GetAddress(0), port);
        apUdpClient.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send a very large number of packets
        apUdpClient.SetAttribute("Interval", TimeValue(packetInterval));
        apUdpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        ApplicationContainer apClientApps = apUdpClient.Install(apNode);
        apClientApps.Start(Seconds(1.0)); // Start after 1 second to allow Wi-Fi association
        apClientApps.Stop(Seconds(simulationTime - 0.1)); // Stop slightly before simulation end
    }

    if (trafficDirection == "StaToAp" || trafficDirection == "Both") {
        // Server on AP (receives traffic)
        UdpEchoServerHelper apUdpServer(port);
        ApplicationContainer apServerApps = apUdpServer.Install(apNode);
        apServerApps.Start(Seconds(0.0));
        apServerApps.Stop(Seconds(simulationTime));

        // Client on STA (sends traffic to AP)
        UdpClientHelper staUdpClient(apInterfaces.GetAddress(0), port);
        staUdpClient.SetAttribute("MaxPackets", UintegerValue(4294967295U));
        staUdpClient.SetAttribute("Interval", TimeValue(packetInterval));
        staUdpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        ApplicationContainer staClientApps = staUdpClient.Install(staNode);
        staClientApps.Start(Seconds(1.0)); // Start after 1 second to allow Wi-Fi association
        staClientApps.Stop(Seconds(simulationTime - 0.1)); // Stop slightly before simulation end
    }

    // 10. Setup Flow Monitor for throughput measurement
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.Install(nodes);

    // 11. Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // 12. Collect and print throughput statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalAggregateThroughputMbps = 0;
    for (auto const& [flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        
        std::cout << "--- Flow " << flowId << " (" << t.sourceAddress << ":" << t.sourcePort 
                  << " -> " << t.destinationAddress << ":" << t.destinationPort << ") ---\n";
        std::cout << "  Tx Bytes: " << flowStats.txBytes << "\n";
        std::cout << "  Rx Bytes: " << flowStats.rxBytes << "\n";
        std::cout << "  Tx Packets: " << flowStats.txPackets << "\n";
        std::cout << "  Rx Packets: " << flowStats.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flowStats.lostPackets << "\n";
        
        if (flowStats.rxBytes > 0 && simulationTime > 0) {
            // Calculate throughput in Mbps: (bytes * 8 bits/byte) / (time in seconds * 1,000,000 bits/Mbps)
            double throughput = (flowStats.rxBytes * 8.0) / (simulationTime * 1000000.0); 
            std::cout << "  Throughput: " << throughput << " Mbps\n";
            totalAggregateThroughputMbps += throughput;
        } else {
            std::cout << "  Throughput: 0 Mbps (No data received or simulation time is zero)\n";
        }
    }
    std::cout << "\nTotal Aggregate Throughput Across All Flows: " << totalAggregateThroughputMbps << " Mbps\n";

    Simulator::Destroy();
    return 0;
}