#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/stats-module.h"

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiVhtErrorModelValidation");

/**
 * @brief Runs a single simulation point to calculate Frame Success Rate (FSR)
 *        for a given error model, MCS, and SNR.
 *
 * @param modelName The string name of the error rate model (e.g., "ns3::YansErrorRateModel").
 * @param mcs The VHT MCS index (0-9).
 * @param snr The target Signal-to-Noise Ratio in dB.
 * @param payloadSize The payload size of application packets in bytes.
 * @param nFrames The number of frames to send for this simulation point.
 * @param txPower The transmit power of the sender in dBm.
 * @param noiseFloor The calculated noise floor of the receiver in dBm.
 * @return The calculated Frame Success Rate (FSR).
 */
double RunSingleSimulationPoint(std::string modelName, int mcs, double snr, uint32_t payloadSize, uint32_t nFrames, double txPower, double noiseFloor)
{
    // Ensure a clean slate for each simulation run
    Simulator::Destroy();

    // Create two nodes: a sender and a receiver
    NodeContainer nodes;
    nodes.Create(2);

    // Install mobility model - nodes are static
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Create a YansWifiChannel with no propagation loss to directly control RxPower via RxGain
    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    channel->SetPropagationLossModel(CreateObject<NoPropagationLossModel>());

    // Configure YansWifiPhyHelper for VHT
    YansWifiPhyHelper phyHelper;
    phyHelper.SetChannel(channel);
    // Set fixed Tx Power for both nodes
    phyHelper.Set("TxPowerStart", DoubleValue(txPower));
    phyHelper.Set("TxPowerEnd", DoubleValue(txPower));
    // Set the noise figure (default is 7 dB)
    phyHelper.Set("NoiseFigure", DoubleValue(7.0));

    // Set the specific error rate model for this simulation point
    phyHelper.SetErrorRateModel(ObjectFactory(modelName).Create<ErrorRateModel>());

    // Configure VhtWifiMacHelper
    WifiMacHelper macHelper;
    macHelper.SetType("ns3::VhtWifiMac",
                      "Ssid", SsidValue(Ssid("ns3-wifi")),
                      "ActiveProbing", BooleanValue(false),
                      "QosSupported", BooleanValue(false));

    // Set 802.11ac standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    // Configure ConstantRateWifiManager to ensure fixed MCS
    // This needs to be set globally for WifiRemoteStationManager.
    Config::SetDefault("ns3::WifiRemoteStationManager", StringValue("ns3::ConstantRateWifiManager"));
    // Set the data mode and control mode for the constant rate manager based on the current MCS.
    // Assuming 1 spatial stream (Nss1) for VHT MCS values 0-9.
    Config::SetDefault("ns3::ConstantRateWifiManager::DataMode", StringValue("VhtMcs" + std::to_string(mcs) + ":VhtNss1"));
    Config::SetDefault("ns3::ConstantRateWifiManager::ControlMode", StringValue("VhtMcs" + std::to_string(mcs) + ":VhtNss1"));
    
    // Disable RTS/CTS, fragmentation, and aggregation for cleaner error model validation
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(2200));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2200));
    Config::SetDefault("ns3::VhtWifiMac::MaxMsduAggregationSize", UintegerValue(0));
    Config::SetDefault("ns3::VhtWifiMac::MaxMpduAggregationSize", UintegerValue(0));

    // Install Wifi devices on nodes
    NetDeviceContainer devices;
    devices = wifi.Install(phyHelper, macHelper, nodes);

    // Get the PHY device on the receiver node to set RxGain
    Ptr<YansWifiPhy> receiverPhy = DynamicCast<YansWifiPhy>(devices.Get(1)->GetObject<WifiNetDevice>()->GetPhy());

    // Calculate required RxGain to achieve the target SNR
    // SNR = RxPower - NoiseFloor
    // RxPower = TxPower - PathLoss + RxGain
    // With PathLoss = 0, RxPower = TxPower + RxGain
    // So, RxGain = SNR + NoiseFloor - TxPower
    double requiredRxGain = snr + noiseFloor - txPower;
    receiverPhy->SetRxGain(requiredRxGain);

    // Configure IP addresses for the nodes
    InternetStackHelper internet;
    internet.Install(nodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure application: OnOffApplication as sender, PacketSink as receiver
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0)); // Sink starts immediately
    sinkApps.Stop(Seconds(2.0));  // Ensure sink runs slightly longer than sender

    OnOffHelper onoffHelper("ns3::UdpSocketFactory", sinkAddress);
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoffHelper.SetAttribute("MaxBytes", UintegerValue(nFrames * payloadSize)); // Sender sends exactly N frames
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));  // Set very high data rate to avoid bottlenecking

    ApplicationContainer sourceApps = onoffHelper.Install(nodes.Get(0));
    sourceApps.Start(Seconds(0.01)); // Sender starts slightly after sink
    sourceApps.Stop(Seconds(1.5));   // Give enough time for N frames to be sent

    // Run the simulation for this single point
    Simulator::Run();

    // Get statistics from the PacketSink
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    uint64_t totalRxBytes = sink->GetTotalRxBytes();
    uint32_t receivedFrames = totalRxBytes / payloadSize;

    double fsr = static_cast<double>(receivedFrames) / nFrames;

    // Log the result for this point
    NS_LOG_INFO("    Model: " << modelName.substr(5) << ", MCS: " << mcs << ", SNR: " << snr << " dB, RxGain: " << requiredRxGain << " dB, FSR: " << fsr);
    
    return fsr;
}


int main(int argc, char *argv[])
{
    // Enable logging components for debugging and information
    LogComponentEnable("WifiVhtErrorModelValidation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_WARN);
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);
    LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
    LogComponentEnable("VhtWifiMac", LOG_LEVEL_INFO);
    LogComponentEnable("ConstantRateWifiManager", LOG_LEVEL_INFO);

    // --- Configurable Parameters ---
    uint32_t payloadSize = 1024; // Default payload size of application packets in bytes
    uint32_t nFrames = 1000;     // Default number of frames to send for each data point
    double txPower = 10.0;       // Fixed transmit power in dBm

    // Parse command line arguments for frame size and number of frames
    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "Payload size of application packets in bytes", payloadSize);
    cmd.AddValue("nFrames", "Number of frames to send per SNR point", nFrames);
    cmd.Parse(argc, argv);

    // --- Calculate Noise Floor ---
    // Standard thermal noise calculation (P_noise = k * T * B) + Noise Figure
    // k (Boltzmann constant) = 1.380649e-23 J/K
    // T (Temperature) = 290 K (25 degrees Celsius)
    // B (Bandwidth) = 80e6 Hz (80 MHz, typical for VHT)
    // Noise Figure = 7 dB (default for YansWifiPhy)
    double k = 1.380649e-23;
    double T = 290.0;
    double B = 80e6;  // 80 MHz channel bandwidth
    double noiseFigure = 7.0; // dB
    double noiseFloor = 10 * std::log10(k * T * B / 1e-3) + noiseFigure; // Result in dBm

    NS_LOG_INFO("Calculated Noise Floor (80MHz, 7dB NF): " << noiseFloor << " dBm");

    // --- List of Error Rate Models to Validate ---
    std::vector<std::string> errorModels = {
        "ns3::YansErrorRateModel",
        "ns3::NistErrorRateModel",
        "ns3::TableBasedErrorRateModel"
    };

    // --- Main Loop: Iterate through each Error Model ---
    for (const std::string &modelName : errorModels)
    {
        NS_LOG_INFO("=====================================================================");
        NS_LOG_INFO("Starting validation for Error Model: " << modelName);
        NS_LOG_INFO("=====================================================================");

        // Setup Gnuplot object for the current error model's results
        std::string plotFileName = "fsr-vs-snr-" + modelName.substr(5) + ".plt"; // Remove "ns3::" prefix for file name
        std::string plotTitle = "Frame Success Rate vs. SNR (" + modelName.substr(5) + ")";
        std::string dataFileName = "fsr-vs-snr-" + modelName.substr(5) + ".txt";

        Gnuplot plot(dataFileName);
        plot.SetTitle(plotTitle);
        plot.SetXLabel("SNR (dB)");
        plot.SetYLabel("Frame Success Rate");
        plot.AppendExtra("set yrange [0:1]"); // FSR ranges from 0 to 1
        plot.AppendExtra("set grid");         // Add grid for better readability

        // --- Inner Loop: Iterate through VHT MCS values (0-9) ---
        // The problem description specifies "excluding MCS 9 for 20 MHz channels".
        // Since this simulation uses an 80 MHz channel (as VHT rates are typically used with wider channels),
        // MCS 9 is valid and included in the test.
        for (int mcs = 0; mcs <= 9; ++mcs)
        {
            NS_LOG_INFO("  -------------------------------------------------------------");
            NS_LOG_INFO("  Testing MCS: " << mcs);
            NS_LOG_INFO("  -------------------------------------------------------------");

            GnuplotDataSet dataSet;
            dataSet.SetTitle("MCS " + std::to_string(mcs)); // Title for this MCS's line in the plot
            dataSet.SetStyle("linespoints");                 // Plot style: lines with points
            dataSet.SetExtra("pointtype 1");                 // Specific point style

            // --- Innermost Loop: Iterate through SNR values (-5 dB to 30 dB) ---
            for (double snr = -5.0; snr <= 30.0; snr += 1.0)
            {
                // Call the helper function to run a self-contained mini-simulation
                // and obtain the Frame Success Rate for the current (model, MCS, SNR) point.
                double fsr = RunSingleSimulationPoint(modelName, mcs, snr, payloadSize, nFrames, txPower, noiseFloor);
                dataSet.Add(snr, fsr); // Add the SNR and FSR data point to the dataset
            } // End SNR loop
            plot.AddDataSet(dataSet); // Add the completed dataset for this MCS to the overall plot
        } // End MCS loop

        // Generate the Gnuplot output files (data file and plot script)
        plot.GenerateOutput(std::cout); // Output Gnuplot script to console (for quick check)
        std::ofstream plotFile(plotFileName.c_str());
        plot.GenerateOutput(plotFile); // Output Gnuplot script to .plt file
        plotFile.close();
    } // End Error Model loop

    // Final cleanup (though Simulator::Destroy() is called at the beginning of each RunSingleSimulationPoint)
    Simulator::Destroy();

    return 0;
}