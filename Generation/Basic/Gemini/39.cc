#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/log-distance-propagation-loss-model.h"
#include "ns3/minstrel-wifi-manager.h"
#include "ns3/ht-wifi-phy.h" // Required for 802.11n PHY
#include <vector>
#include <fstream>
#include <string>
#include <tuple>   // For std::tuple
#include <cmath>   // For std::sqrt (though not explicitly used for sqrt, common include)

// Use ns-3 namespace to avoid verbose ns3:: prefix
using namespace ns3;

// Define a log component for this simulation script
NS_LOG_COMPONENT_DEFINE("MinstrelSimulation");

// Global variables for statistics collection
Ptr<PacketSink> g_staPacketSink; // Pointer to the PacketSink application on the STA
std::vector<std::tuple<double, double, double>> g_stats; // Stores (Time, Distance, Throughput) tuples
Ptr<MobilityModel> g_staMobility; // Pointer to STA's mobility model
Ptr<MobilityModel> g_apMobility;  // Pointer to AP's mobility model

/**
 * @brief Callback function to collect simulation statistics periodically.
 *
 * This function is scheduled to run at regular intervals to record the current
 * simulation time, the distance between the AP and STA, and the throughput
 * received by the STA during the last interval.
 */
void CollectStatistics()
{
    // Get current simulation time
    double currentTime = Simulator::Now().GetSeconds();

    // Get current positions of AP and STA and calculate the distance
    Vector apPos = g_apMobility->GetPosition();
    Vector staPos = g_staMobility->GetPosition();
    double currentDistance = apPos.GetDistanceFrom(staPos);

    // Calculate throughput based on received bytes by the PacketSink
    static uint64_t lastTotalRxBytes = 0;
    static double lastThroughputTime = 0.0;

    uint64_t currentTotalRxBytes = g_staPacketSink->GetTotalRxBytes();
    double intervalRxBytes = currentTotalRxBytes - lastTotalRxBytes;
    double intervalTime = currentTime - lastThroughputTime;

    double currentThroughputMbps = 0.0;
    // Avoid division by zero and ensure a valid interval for throughput calculation
    if (intervalTime > 0)
    {
        currentThroughputMbps = (intervalRxBytes * 8.0) / (intervalTime * 1e6); // Convert bytes/s to Mbps
    }

    // Store the collected statistics
    g_stats.emplace_back(currentTime, currentDistance, currentThroughputMbps);

    // Update static variables for the next collection interval
    lastTotalRxBytes = currentTotalRxBytes;
    lastThroughputTime = currentTime;
}

int main(int argc, char *argv[])
{
    // 1. Define command-line arguments and their default values
    double simulationTime = 30.0;   // Total simulation duration in seconds
    double initialDistance = 1.0;   // Initial distance of STA from AP in meters
    double distanceStep = 5.0;      // Distance STA moves per timePerStep (m)
    double timePerStep = 1.0;       // Duration of each step in seconds (s)
    uint32_t packetSize = 1472;     // UDP packet size (1500 - 20 (IP) - 8 (UDP) = 1472 bytes)
    std::string dataRate = "400Mbps"; // CBR data rate from AP to STA
    std::string apRateManager = "ns3::IdealWifiManager"; // AP's rate adaptation strategy
    std::string staRateManager = "ns3::MinstrelWifiManager"; // STA's rate adaptation strategy
    double apTxPower = 20.0;        // AP transmit power in dBm
    double staTxPower = 20.0;       // STA transmit power in dBm
    bool logRateChanges = false;    // Enable Minstrel's internal logging for rate changes

    // Set up command-line argument parser
    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("initialDistance", "Initial distance of STA from AP in meters", initialDistance);
    cmd.AddValue("distanceStep", "Distance STA moves per timePerStep (m)", distanceStep);
    cmd.AddValue("timePerStep", "Time duration for each step (s)", timePerStep);
    cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
    cmd.AddValue("dataRate", "CBR data rate (e.g., 400Mbps)", dataRate);
    cmd.AddValue("apRateManager", "AP's rate adaptation manager", apRateManager);
    cmd.AddValue("staRateManager", "STA's rate adaptation manager", staRateManager);
    cmd.AddValue("apTxPower", "AP transmit power in dBm", apTxPower);
    cmd.AddValue("staTxPower", "STA transmit power in dBm", staTxPower);
    cmd.AddValue("logRateChanges", "Enable Minstrel internal rate change logging (LOG_LEVEL_INFO)", logRateChanges);
    cmd.Parse(argc, argv);

    // Input validation for parameters
    if (packetSize > 1472)
    {
        NS_LOG_WARN("Packet size " << packetSize << " bytes is greater than typical MTU (1500 - IP_HDR - UDP_HDR = 1472). May cause fragmentation.");
    }
    if (initialDistance < 0)
    {
        initialDistance = 0; // Distance cannot be negative
    }
    if (distanceStep < 0)
    {
        distanceStep = 0; // Movement step cannot be negative
    }
    if (timePerStep <= 0)
    {
        timePerStep = 1.0; // Avoid division by zero for velocity calculation
        NS_LOG_WARN("timePerStep must be greater than 0. Setting to default 1.0s.");
    }

    // Enable specific log components if requested for debugging/analysis
    if (logRateChanges)
    {
        LogComponentEnable("MinstrelWifiManager", LOG_LEVEL_INFO);
        // LogComponentEnable("WifiPhy", LOG_LEVEL_DEBUG); // Uncomment for more detailed PHY logs
        NS_LOG_UNCOND("MinstrelWifiManager logging enabled. Rate changes will be printed to stdout.");
    }
    else
    {
        LogComponentDisable("MinstrelWifiManager", LOG_LEVEL_INFO);
        LogComponentDisable("WifiPhy", LOG_LEVEL_DEBUG);
    }

    // 2. Node Creation
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNode;
    staNode.Create(1);

    // 3. Mobility Setup
    // AP uses ConstantPositionMobilityModel at (0, 0, 0)
    MobilityHelper mobility;
    Ptr<ConstantPositionMobilityModel> apCpModel = CreateObject<ConstantPositionMobilityModel>();
    apCpModel->SetPosition(Vector(0.0, 0.0, 0.0));
    apNode.Get(0)->AggregateObject(apCpModel);
    g_apMobility = apCpModel; // Store for global access

    // STA uses ConstantVelocityMobilityModel to move away
    Ptr<ConstantVelocityMobilityModel> staCvModel = CreateObject<ConstantVelocityMobilityModel>();
    staCvModel->SetPosition(Vector(initialDistance, 0.0, 0.0)); // Initial position
    // Calculate velocity based on desired distance step and time per step
    staCvModel->SetVelocity(Vector(distanceStep / timePerStep, 0.0, 0.0));
    staNode.Get(0)->AggregateObject(staCvModel);
    g_staMobility = staCvModel; // Store for global access

    // 4. Wi-Fi Configuration
    WifiHelper wifi;
    // Set Wi-Fi standard to 802.11n (5GHz band recommended for higher rates)
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    // Configure the Wi-Fi channel with propagation loss and delay models
    YansWifiChannelHelper channel;
    // LogDistancePropagationLossModel simulates signal attenuation with distance
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue(3.0)); // Exponent of 3.0 for faster signal drop
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    // Configure the PHY layer (physical layer)
    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    // Set default transmit power for all devices installed with this PHY helper
    phy.Set("TxPowerStart", DoubleValue(apTxPower)); // Use AP's Tx power as default for convenience
    phy.Set("TxPowerEnd", DoubleValue(apTxPower));
    phy.Set("RxSensitivity", DoubleValue(-98.0)); // Default receiver sensitivity

    // Configure the MAC layer (Media Access Control)
    WifiMacHelper mac;
    Ssid ssid = Ssid("minstrel-simulation-ssid"); // Common SSID for AP and STA

    // Install Wi-Fi device on AP node
    wifi.SetRemoteStationManager(apRateManager); // Set AP's rate adaptation manager
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Install Wi-Fi device on STA node
    wifi.SetRemoteStationManager(staRateManager); // Set STA's rate adaptation manager (Minstrel)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    // Explicitly set Tx power for each device (if different from default or to be sure)
    apDevice.Get(0)->Get<WifiNetDevice>()->GetPhy()->SetTxPower(apTxPower);
    staDevice.Get(0)->Get<WifiNetDevice>()->GetPhy()->SetTxPower(staTxPower);

    // 5. Install Internet Stack and Assign IP Addresses
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0"); // Define IP network
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);

    // 6. Setup Application Layer (UDP Traffic AP -> STA)
    uint16_t port = 9; // Standard discard port

    // STA: UDP Server (PacketSink to receive data)
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(staNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));
    g_staPacketSink = DynamicCast<PacketSink>(serverApp.Get(0)); // Store pointer for stats collection

    // AP: UDP Client (to send CBR traffic to STA)
    // Calculate interval to achieve desired data rate: PacketSize * 8 bits/byte / DataRate
    Time interval = Seconds(packetSize * 8.0 / DataRate(dataRate).GetBitRate());
    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0)); // Send indefinitely
    client.SetAttribute("Interval", TimeValue(interval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(apNode.Get(0));
    clientApp.Start(Seconds(1.0)); // Start client after a 1s delay
    clientApp.Stop(Seconds(simulationTime));

    // 7. Schedule Data Collection
    // Schedule the CollectStatistics function to run periodically throughout the simulation
    double collectionInterval = timePerStep;
    for (double t = 0.0; t <= simulationTime; t += collectionInterval)
    {
        Simulator::Schedule(Seconds(t), &CollectStatistics);
    }
    // Ensure data is collected at the very end of the simulation as well
    Simulator::Schedule(Seconds(simulationTime), &CollectStatistics);

    // 8. Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    // 9. Output Results to Gnuplot Files
    std::ofstream dataFile("throughput_vs_distance.dat");
    // Write header for the data file
    dataFile << "# Time (s)\tDistance (m)\tThroughput (Mbps)\n";
    for (const auto &entry : g_stats)
    {
        dataFile << std::get<0>(entry) << "\t" // Time
                 << std::get<1>(entry) << "\t" // Distance
                 << std::get<2>(entry) << "\n"; // Throughput
    }
    dataFile.close();

    std::ofstream plotFile("throughput_vs_distance.plt");
    plotFile << "# Gnuplot script for Throughput vs. Distance\n";
    plotFile << "set title 'Throughput vs. Distance with Minstrel Rate Adaptation'\n";
    plotFile << "set xlabel 'Distance (m)'\n";
    plotFile << "set ylabel 'Throughput (Mbps)'\n";
    plotFile << "set grid\n";
    plotFile << "set terminal pngcairo size 1024,768\n"; // Optional: output to PNG
    plotFile << "set output 'throughput_vs_distance.png'\n"; // Optional: output file name
    plotFile << "plot 'throughput_vs_distance.dat' using 2:3 with linespoints title 'Throughput'\n";
    plotFile.close();

    NS_LOG_UNCOND("Simulation completed. Data saved to throughput_vs_distance.dat");
    NS_LOG_UNCOND("Gnuplot script saved to throughput_vs_distance.plt");
    NS_LOG_UNCOND("To generate plot, run: gnuplot throughput_vs_distance.plt");

    return 0;
}