#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>

// Using ns3 namespace for convenience in this single file example
using namespace ns3;

// Global simulation parameters
uint32_t g_nNonHtSta = 5;      // Number of 802.11b/g stations
uint32_t g_nHtSta = 5;         // Number of 802.11n stations
double g_simTime = 10.0;       // Simulation time in seconds
uint16_t g_udpPort = 9;        // UDP application port
uint16_t g_tcpPort = 10;       // TCP application port
std::string g_dataRate = "10Mbps"; // Data rate for UDP OnOffApplication
std::vector<uint32_t> g_payloadSizes = {500, 1000, 1400}; // Various payload sizes in bytes

/**
 * @brief Runs a single NS-3 simulation scenario with specified configurations.
 *
 * @param trafficType String indicating "UDP" or "TCP" traffic.
 * @param erpProtection Boolean, true to enable ERP protection mode.
 * @param shortPPDUMode Boolean, true to enable short PPDU format (Greenfield for HT, Short Preamble for non-HT).
 * @param shortSlotTime Boolean, true to enable short slot time.
 * @param payloadSize Integer, the size of application payload in bytes.
 */
void runSimulation(std::string trafficType, bool erpProtection, bool shortPPDUMode, bool shortSlotTime, uint32_t payloadSize)
{
    // Clean up previous simulation state to ensure independent runs
    Simulator::Destroy();

    // 1. Create Nodes (Access Point and Stations)
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer nonHtStaNodes;
    nonHtStaNodes.Create(g_nNonHtSta);
    NodeContainer htStaNodes;
    htStaNodes.Create(g_nHtSta);

    // 2. Configure Mobility (fixed positions for simplicity)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(5.0),    // Starting X coordinate for grid
                                  "MinY", DoubleValue(5.0),    // Starting Y coordinate for grid
                                  "DeltaX", DoubleValue(10.0), // Spacing between nodes in X
                                  "DeltaY", DoubleValue(10.0), // Spacing between nodes in Y
                                  "GridWidth", UintegerValue(4), // Number of columns in the grid
                                  "LayoutType", StringValue("RowFirst")); // Fill rows first
    
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    
    // Install mobility for stations (AP set manually)
    mobility.Install(nonHtStaNodes);
    mobility.Install(htStaNodes);
    
    // Set AP at origin (0,0,0)
    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    // 3. Configure WiFi components
    WifiHelper wifi;
    // Set standard to 802.11n_2.4GHz, which allows for coexistence of b/g/n devices
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    YansWifiPhyHelper phy;
    phy.SetChannel(YansWifiChannelHelper::Default().Create());
    phy.Set("TxPowerStart", DoubleValue(15)); // Set Tx Power for all devices
    phy.Set("TxPowerEnd", DoubleValue(15));   // Set Tx Power for all devices

    // MAC Layer Configuration for AP (802.11n HT MAC)
    HtWifiMacHelper apHtWifiMac = HtWifiMacHelper::Default();
    apHtWifiMac.SetType ("ns3::ApWifiMac", "QosSupported", BooleanValue (true));
    apHtWifiMac.Set ("ERPProtection", BooleanValue (erpProtection)); // Enable/disable ERP protection
    apHtWifiMac.Set ("ShortSlotTime", BooleanValue (shortSlotTime)); // Enable/disable short slot time
    if (shortPPDUMode) {
        apHtWifiMac.SetPreambleType(HtWifiMacHelper::HT_GREENFIELD); // Short PPDU for HT
    } else {
        apHtWifiMac.SetPreambleType(HtWifiMacHelper::HT_MIXED);      // Long PPDU for HT
    }

    // MAC Layer Configuration for HT Stations (802.11n HT MAC)
    HtWifiMacHelper staHtWifiMac = HtWifiMacHelper::Default();
    staHtWifiMac.SetType ("ns3::StaWifiMac", "QosSupported", BooleanValue (true),
                          "ActiveProbing", BooleanValue (false)); // Disable active probing for simplicity
    staHtWifiMac.Set ("ERPProtection", BooleanValue (erpProtection));
    staHtWifiMac.Set ("ShortSlotTime", BooleanValue (shortSlotTime));
    if (shortPPDUMode) {
        staHtWifiMac.SetPreambleType(HtWifiMacHelper::HT_GREENFIELD);
    } else {
        staHtWifiMac.SetPreambleType(HtWifiMacHelper::HT_MIXED);
    }

    // MAC Layer Configuration for Non-HT Stations (802.11g compatible Nqos MAC)
    NqosWifiMacHelper nonHtWifiMac = NqosWifiMacHelper::Default();
    nonHtWifiMac.SetType ("ns3::StaWifiMac", "QosSupported", BooleanValue (true),
                          "ActiveProbing", BooleanValue (false));
    nonHtWifiMac.Set ("ERPProtection", BooleanValue (erpProtection));
    nonHtWifiMac.Set ("ShortSlotTime", BooleanValue (shortSlotTime));
    if (shortPPDUMode) {
        nonHtWifiMac.SetPreambleType(WifiMacHelper::SHORT_PREAMBLE); // Short Preamble for non-HT
    } else {
        nonHtWifiMac.SetPreambleType(WifiMacHelper::LONG_PREAMBLE);  // Long Preamble for non-HT
    }
    
    // Install WiFi devices on nodes
    NetDeviceContainer apDevices = wifi.Install (phy, apHtWifiMac, apNode);
    NetDeviceContainer htStaDevices = wifi.Install (phy, staHtWifiMac, htStaNodes);
    NetDeviceContainer nonHtStaDevices = wifi.Install (phy, nonHtWifiMac, nonHtStaNodes);

    // 4. Install Internet Stack and assign IP Addresses
    InternetStackHelper internet;
    internet.Install (apNode);
    internet.Install (htStaNodes);
    internet.Install (nonHtStaNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = ipv4.Assign (apDevices);
    Ipv4InterfaceContainer htStaInterfaces = ipv4.Assign (htStaDevices);
    Ipv4InterfaceContainer nonHtStaInterfaces = ipv4.Assign (nonHtStaDevices);

    // 5. Configure Applications (All STAs send traffic to the AP)
    ApplicationContainer clientApps;
    Ipv4Address apAddress = apInterfaces.GetAddress(0); // Get AP's IP address

    // Packet Sink on the AP
    PacketSinkHelper sinkHelper (trafficType == "UDP" ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory",
                                 InetSocketAddress (Ipv4Address::GetAny (), trafficType == "UDP" ? g_udpPort : g_tcpPort));
    ApplicationContainer sinkApp = sinkHelper.Install (apNode);
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (g_simTime + 1.0));

    // Client applications (On/Off for UDP, BulkSend for TCP)
    // Install on HT Stations
    for (uint32_t i = 0; i < g_nHtSta; ++i)
    {
        AddressValue remoteAddress (InetSocketAddress (apAddress, trafficType == "UDP" ? g_udpPort : g_tcpPort));
        if (trafficType == "UDP") {
            OnOffHelper onoff ("ns3::UdpSocketFactory", Address ());
            onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]")); // Always On
            onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));// Always On
            onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            onoff.SetAttribute ("DataRate", DataRateValue (DataRate (g_dataRate)));
            onoff.SetRemote(remoteAddress);
            clientApps.Add (onoff.Install (htStaNodes.Get(i)));
        } else { // TCP
            BulkSendHelper bulk ("ns3::TcpSocketFactory", Address ());
            bulk.SetAttribute ("MaxBytes", UintegerValue (0)); // Send until simulation ends
            bulk.SetRemote(remoteAddress);
            clientApps.Add (bulk.Install (htStaNodes.Get(i)));
        }
    }
    // Install on Non-HT Stations
    for (uint32_t i = 0; i < g_nNonHtSta; ++i)
    {
        AddressValue remoteAddress (InetSocketAddress (apAddress, trafficType == "UDP" ? g_udpPort : g_tcpPort));
        if (trafficType == "UDP") {
            OnOffHelper onoff ("ns3::UdpSocketFactory", Address ());
            onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
            onoff.SetAttribute ("DataRate", DataRateValue (DataRate (g_dataRate)));
            onoff.SetRemote(remoteAddress);
            clientApps.Add (onoff.Install (nonHtStaNodes.Get(i)));
        } else { // TCP
            BulkSendHelper bulk ("ns3::TcpSocketFactory", Address ());
            bulk.SetAttribute ("MaxBytes", UintegerValue (0));
            bulk.SetRemote(remoteAddress);
            clientApps.Add (bulk.Install (nonHtStaNodes.Get(i)));
        }
    }

    clientApps.Start (Seconds (1.0)); // Start client apps after 1 second
    clientApps.Stop (Seconds (g_simTime + 1.0)); // Stop client apps 1 second after simulation end

    // 6. Setup FlowMonitor to collect statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // 7. Run Simulation
    Simulator::Stop (Seconds (g_simTime + 1.0)); // Ensure simulation runs for full time
    Simulator::Run ();

    // 8. Collect and print results
    double totalThroughputMbps = 0.0;
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
    // Calculate total received bytes and convert to Mbps
    totalThroughputMbps = sink->GetTotalRxBytes() * 8.0 / (g_simTime * 1000000.0);

    // Print results for the current scenario
    std::cout << std::left << std::setw(8) << trafficType
              << std::left << std::setw(12) << (erpProtection ? "Enabled" : "Disabled")
              << std::left << std::setw(12) << (shortPPDUMode ? "Short" : "Long")
              << std::left << std::setw(12) << (shortSlotTime ? "Short" : "Long")
              << std::left << std::setw(10) << payloadSize
              << std::right << std::setw(20) << std::fixed << std::setprecision(2) << totalThroughputMbps
              << std::endl;
}

int main(int argc, char *argv[])
{
    // Optional: Enable logging for specific ns-3 components for debugging
    // LogComponentEnable ("WifiMixedNetworkPerformance", LOG_LEVEL_INFO);
    // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("WifiMac", LOG_LEVEL_INFO);
    // LogComponentEnable ("YansWifiPhy", LOG_LEVEL_INFO);

    // Set default attributes for common WiFi settings
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
    // Set RTS/CTS threshold high by default so ERPProtection primarily controls RTS/CTS usage
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200")); 

    // Print header for results table
    std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << "Mixed WiFi Network Simulation Results (HT STAs: " << g_nHtSta << ", Non-HT STAs: " << g_nNonHtSta << ", Simulation Time: " << g_simTime << "s)" << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(8) << "Traffic"
              << std::left << std::setw(12) << "ERP Prot."
              << std::left << std::setw(12) << "PPDU"
              << std::left << std::setw(12) << "Slot Time"
              << std::left << std::setw(10) << "Payload"
              << std::right << std::setw(20) << "Throughput (Mbps)"
              << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------------" << std::endl;

    // Define the different scenarios to test
    std::vector<std::string> trafficTypes = {"UDP", "TCP"};
    std::vector<bool> erpProtectionModes = {false, true};
    std::vector<bool> shortPPDUModes = {false, true}; // Corresponds to HT_GREENFIELD/HT_MIXED for HT, Short/Long for non-HT
    std::vector<bool> shortSlotTimes = {false, true};

    // Iterate through all combinations of scenarios and run simulations
    for (const auto& trafficType : trafficTypes)
    {
        for (bool erpProtection : erpProtectionModes)
        {
            for (bool shortPPDUMode : shortPPDUModes)
            {
                for (bool shortSlotTime : shortSlotTimes)
                {
                    for (uint32_t payloadSize : g_payloadSizes)
                    {
                        runSimulation(trafficType, erpProtection, shortPPDUMode, shortSlotTime, payloadSize);
                    }
                }
            }
        }
    }

    std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------------" << std::endl;

    return 0;
}