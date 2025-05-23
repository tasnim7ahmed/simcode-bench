#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/stats-module.h"
#include "ns3/gnuplot-helper.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cmath> // For std::round

using namespace ns3;

// Global variables for simulation parameters and data collection
std::string g_rateManagerType = "ParfWifiManager";
uint32_t g_rtsCtsThreshold = 2200; // Default: 2200 bytes, 0 for always on
double g_apTxPower = 20.0; // dBm
double g_staTxPower = 20.0; // dBm (Tx power for STA, though STA is receiver here)
double g_distanceIncrementPerSecond = 1.0; // meters/second
double g_simulationTime = 60.0; // seconds
std::string g_udpCbrRate = "54Mbps";
std::string g_outputDir = "."; // Default output directory
std::string g_prefix = "wifi-comp"; // Default prefix for output files

// Specific output file stream for per-second logs
std::ofstream g_txLogFileStream;

// Node and Device Pointers
Ptr<Node> g_apNode;
Ptr<Node> g_staNode;
Ptr<ConstantVelocityMobilityModel> g_staMobility;
Ptr<WifiNetDevice> g_apWifiDevice;
Ptr<PacketSink> g_packetSink;

// Data structures for plotting (averaged per distance bin)
// distance (rounded) -> sum of received bytes in that bin
std::map<double, uint64_t> g_totalRxBytesPerDistanceBin;
// distance (rounded) -> count of seconds for each distance bin (for averaging)
std::map<double, uint32_t> g_countRxBytesPerDistanceBin;
// distance (rounded) -> list of tx powers recorded at that distance
std::map<double, std::vector<double>> g_txPowerPerDistanceBin;

uint64_t g_previousTotalRxBytes = 0; // To calculate delta bytes for throughput

/**
 * \brief Trace sink for AP's Tx Power changes.
 *
 * This function is called whenever the WifiPhy's TxPower attribute changes.
 * It records the new power value along with the current distance to the STA.
 */
void
TxPowerTrace(Ptr<const WifiPhy> phy, double oldPower, double newPower)
{
    // Get current distance of STA from AP
    Vector apPos = g_apNode->GetObject<ConstantPositionMobilityModel>()->GetPosition();
    Vector staPos = g_staMobility->GetPosition();
    double currentDistance = apPos.GetDistanceFrom(staPos);
    double roundedDistance = std::round(currentDistance); // Round to nearest integer for binning

    g_txPowerPerDistanceBin[roundedDistance].push_back(newPower);
}

/**
 * \brief Periodic logging and data collection function.
 *
 * This function is scheduled to run every second. It logs current simulation
 * state (time, distance, Tx Rate, Tx Power) to a file and collects data
 * for post-simulation Gnuplot plotting (throughput and Tx power vs. distance).
 */
void
LogCurrentState()
{
    Time currentSimTime = Simulator::Now();
    
    // Get current distance of STA from AP
    Vector apPos = g_apNode->GetObject<ConstantPositionMobilityModel>()->GetPosition();
    Vector staPos = g_staMobility->GetPosition();
    double currentDistance = apPos.GetDistanceFrom(staPos);

    // Log Tx Rate and Power (per second) to a file
    Ptr<WifiPhy> apPhy = g_apWifiDevice->GetPhy();
    // Get the currently used WifiMode (which includes the data rate)
    WifiMode currentTxMode = apPhy->GetMcsWifiMode(); 
    // Get the current TxPower attribute value from the Phy
    double currentTxPower = apPhy->GetTxPower(); 

    g_txLogFileStream << currentSimTime.GetSeconds() << "\t"
                      << currentDistance << "\t"
                      << currentTxMode.GetPsduRate() / 1e6 << "\t" // Convert bps to Mbps
                      << currentTxPower << "\n";

    // Collect throughput data for plotting (averaged per distance bin)
    uint64_t currentTotalRxBytes = g_packetSink->GetTotalReceivedBytes();
    uint64_t deltaRxBytes = currentTotalRxBytes - g_previousTotalRxBytes;
    g_previousTotalRxBytes = currentTotalRxBytes;

    double roundedDistance = std::round(currentDistance); // Round for binning
    g_totalRxBytesPerDistanceBin[roundedDistance] += deltaRxBytes;
    g_countRxBytesPerDistanceBin[roundedDistance]++;

    // Schedule next log if simulation time has not ended
    if (currentSimTime.GetSeconds() + 1.0 <= g_simulationTime)
    {
        Simulator::Schedule(Seconds(1.0), &LogCurrentState);
    }
}

int
main(int argc, char *argv[])
{
    // 1. Command Line Processing
    CommandLine cmd(__FILE__);
    cmd.AddValue("rateManager", "Type of WifiRateManager to use (e.g., ParfWifiManager, AparfWifiManager, RrpaaWifiManager)", g_rateManagerType);
    cmd.AddValue("rtsThreshold", "RTS/CTS threshold in bytes (0 for always on)", g_rtsCtsThreshold);
    cmd.AddValue("apTxPower", "Access Point Tx Power in dBm", g_apTxPower);
    cmd.AddValue("staTxPower", "Station Tx Power in dBm", g_staTxPower);
    cmd.AddValue("distanceInc", "Station movement distance increment per second (meters)", g_distanceIncrementPerSecond);
    cmd.AddValue("simTime", "Simulation time in seconds", g_simulationTime);
    cmd.AddValue("udpRate", "UDP CBR Application Rate", g_udpCbrRate);
    cmd.AddValue("outputDir", "Output directory for simulation results", g_outputDir);
    cmd.AddValue("prefix", "Prefix for output filenames", g_prefix);
    cmd.Parse(argc, argv);

    // 2. Output Filename Setup and Stream Open
    std::string txPowerLogFileName = g_outputDir + "/" + g_prefix + "-tx-log.txt";
    std::string throughputGnuplotFileName = g_outputDir + "/" + g_prefix + "-throughput.plt";
    std::string txPowerGnuplotFileName = g_outputDir + "/" + g_prefix + "-txpower.plt";

    g_txLogFileStream.open(txPowerLogFileName.c_str());
    g_txLogFileStream << "# Time(s)\tDistance(m)\tTxRate(Mbps)\tTxPower(dBm)\n";

    // 3. Create Nodes: one Access Point (AP) and one Station (STA)
    NodeContainer nodes;
    nodes.Create(2);
    g_apNode = nodes.Get(0);
    g_staNode = nodes.Get(1);

    // 4. Install Internet Stack on both nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Mobility Configuration
    MobilityHelper mobility;

    // AP is stationary at (0,0,0)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(g_apNode);
    g_apNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // STA moves away from AP with constant velocity
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(g_staNode);
    g_staMobility = g_staNode->GetObject<ConstantVelocityMobilityModel>();
    g_staMobility->SetPosition(Vector(0.0, 1.0, 0.0)); // Start 1 meter away from AP
    g_staMobility->SetVelocity(Vector(0.0, g_distanceIncrementPerSecond, 0.0));

    // 6. WiFi Configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Using 802.11n as it supports 54Mbps and higher rates
    
    // Set the desired WifiRateManager (rate control mechanism)
    wifi.SetRemoteStationManager("ns3::" + g_rateManagerType, 
                                 "RtsCtsThreshold", UintegerValue(g_rtsCtsThreshold));

    YansWifiChannelHelper channel;
    // Use a simple propagation loss model that directly depends on distance
    channel.SetPropagationLossModel("ns3::RangePropagationLossModel",
                                    "MaxRange", DoubleValue(250.0)); // Max range to avoid disconnects too early
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    // Set initial Tx Power for both nodes. RrpaaWifiManager will adjust this attribute on the AP.
    phy.Set("TxPowerStart", DoubleValue(g_apTxPower)); // Max power for AP
    phy.Set("TxPowerEnd", DoubleValue(g_apTxPower));   // Max power for AP

    WifiMacHelper mac;
    NetDeviceContainer devices;

    // AP MAC configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", Ssid("ns3-wifi"),
                "BeaconInterval", TimeValue(MicroSeconds(102400)));
    devices.Add(wifi.Install(phy, mac, g_apNode));
    g_apWifiDevice = DynamicCast<WifiNetDevice>(devices.Get(0));

    // STA MAC configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", Ssid("ns3-wifi"),
                "ActiveProbing", BooleanValue(false)); // STA does not actively probe in this setup
    devices.Add(wifi.Install(phy, mac, g_staNode));
    Ptr<WifiNetDevice> staWifiDevice = DynamicCast<WifiNetDevice>(devices.Get(1));

    // 7. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(g_apWifiDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staWifiDevice);

    // 8. Traffic Generation (UDP CBR from AP to STA)
    // AP (Sender Application)
    UdpCbrHelper cbrHelper(staInterfaces.GetAddress(0), 9); // STA IP, Port 9
    cbrHelper.SetAttribute("MaxBytes", UintegerValue(0));   // Send indefinitely
    cbrHelper.SetAttribute("PacketSize", UintegerValue(1400)); // Default packet size
    cbrHelper.SetAttribute("Rate", DataRateValue(DataRate(g_udpCbrRate)));

    ApplicationContainer sourceApps = cbrHelper.Install(g_apNode);
    sourceApps.Start(Seconds(1.0)); // Start after 1 second to allow association
    sourceApps.Stop(Seconds(g_simulationTime - 0.1)); // Stop slightly before sim end

    // STA (Receiver Application)
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApps = sinkHelper.Install(g_staNode);
    g_packetSink = DynamicCast<PacketSink>(sinkApps.Get(0)); // Get handle to sink app for byte counting
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(g_simulationTime));

    // 9. Connect Traces and Schedule Periodic Logging
    // Connect TxPower trace for AP's Phy. This attribute is adjusted by RrpaaWifiManager.
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(g_apNode->GetId()) +
                                  "/DeviceList/0/$ns3::WifiNetDevice/Phy/TxPower",
                                  MakeCallback(&TxPowerTrace));
    
    // Schedule the periodic logging function to run every second
    Simulator::Schedule(Seconds(1.0), &LogCurrentState);

    // 10. Run Simulation
    Simulator::Stop(Seconds(g_simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    // 11. Post-Simulation Data Processing for Gnuplot
    Gnuplot2dDataset throughputVsDistanceDataset;
    throughputVsDistanceDataset.SetTitle("Average Throughput vs. Distance");
    throughputVsDistanceDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    throughputVsDistanceDataset.SetExtra("with linespoints pt 7 ps 0.5");

    Gnuplot2dDataset txPowerVsDistanceDataset;
    txPowerVsDistanceDataset.SetTitle("Average Transmit Power vs. Distance");
    txPowerVsDistanceDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    txPowerVsDistanceDataset.SetExtra("with linespoints pt 7 ps 0.5");

    // Process collected throughput data for plotting
    for (auto const& [distance, totalBytes] : g_totalRxBytesPerDistanceBin)
    {
        uint32_t count = g_countRxBytesPerDistanceBin[distance];
        if (count > 0)
        {
            double avgThroughputMbps = (static_cast<double>(totalBytes) * 8.0) / (count * 1e6);
            throughputVsDistanceDataset.Add(distance, avgThroughputMbps);
        }
    }

    // Process collected Tx Power data for plotting
    for (auto const& [distance, powers] : g_txPowerPerDistanceBin)
    {
        if (!powers.empty())
        {
            double sumPower = 0.0;
            for (double p : powers)
            {
                sumPower += p;
            }
            double avgPower = sumPower / powers.size();
            txPowerVsDistanceDataset.Add(distance, avgPower);
        }
    }

    // 12. Generate Gnuplot files
    Gnuplot gnuplotThroughput(throughputGnuplotFileName);
    gnuplotThroughput.SetTitle("Average Throughput vs. Distance");
    gnuplotThroughput.SetXLabel("Distance (m)");
    gnuplotThroughput.SetYLabel("Average Throughput (Mbps)");
    gnuplotThroughput.AddDataset(throughputVsDistanceDataset);
    gnuplotThroughput.GenerateOutput(); // Generates the .plt file

    Gnuplot gnuplotTxPower(txPowerGnuplotFileName);
    gnuplotTxPower.SetTitle("Average Transmit Power vs. Distance");
    gnuplotTxPower.SetXLabel("Distance (m)");
    gnuplotTxPower.SetYLabel("Average Tx Power (dBm)");
    gnuplotTxPower.AddDataset(txPowerVsDistanceDataset);
    gnuplotTxPower.GenerateOutput(); // Generates the .plt file

    // Close the per-second log file stream
    g_txLogFileStream.close();

    return 0;
}