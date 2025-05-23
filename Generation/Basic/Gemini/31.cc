#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot-module.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath> 

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiErrorRateModelValidation");

// Simulation parameters
double g_simDuration = 0.5; // seconds
uint32_t g_frameSize = 1500; // bytes
double g_txPowerDb = 20.0; // dBm
double g_noiseFigureDb = 7.0; // dB (typical receiver noise figure)
double g_thermalNoiseDb = -101.0; // dBm (for 20MHz bandwidth at 290K, i.e., kB * T * B)

// SNR range for plots
double g_minSnrDb = 0.0;
double g_maxSnrDb = 30.0;
double g_snrStepDb = 2.0;

// Max HT MCS to test (0-7 for SISO, 0-15 for 2x2 MIMO, 0-23 for 3x3 MIMO).
// Limiting to 0-7 for demonstration purposes to avoid excessive simulation time.
int g_maxMcs = 7; 

/**
 * @brief Runs a single simulation scenario to get Frame Success Rate for a given MCS, SNR, and error model.
 *
 * This function sets up a minimal 2-node WiFi network (AP and STA, or two ad-hoc nodes),
 * configures it with the specified parameters, and runs a traffic flow to measure FSR.
 *
 * @param mcs The HT MCS value to use.
 * @param targetSnrDb The target SNR in dB for the simulation.
 * @param errorModelTypeId The TypeId of the ErrorRateModel to use.
 * @param txPowerDb The transmit power in dBm.
 * @param noiseFigureDb The noise figure in dB.
 * @param thermalNoiseDb The thermal noise floor in dBm.
 * @param frameSize The size of the frames in bytes.
 * @param simDuration The duration of the simulation in seconds.
 * @return The Frame Success Rate (FSR) as a double.
 */
static double RunSingleSimulation(int mcs,
                                  double targetSnrDb,
                                  TypeId errorModelTypeId,
                                  double txPowerDb,
                                  double noiseFigureDb,
                                  double thermalNoiseDb,
                                  uint32_t frameSize,
                                  double simDuration)
{
    // Ensure cleanup from previous runs of Simulator (crucial for multiple runs in main)
    Simulator::Destroy();

    NS_LOG_INFO("Running simulation for MCS " << mcs 
                                            << ", SNR " << targetSnrDb 
                                            << " dB, Error Model " << errorModelTypeId.GetName());

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Install mobility model (fixed positions at origin)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 3. Setup Wifi Channel and Propagation Loss
    // Calculate required path loss to achieve the target SNR
    // SNR = TxPower - PathLoss - (ThermalNoise + NoiseFigure)
    // PathLoss = TxPower - (ThermalNoise + NoiseFigure) - SNR
    double pathLossDb = txPowerDb - (thermalNoiseDb + noiseFigureDb) - targetSnrDb;
    if (pathLossDb < 0) { 
        // Path loss cannot be negative in a real-world scenario; this means the target SNR is too high for the given Tx/Noise.
        // We cap it at 0 dB to represent ideal conditions (no loss).
        NS_LOG_WARN("Calculated path loss is negative (" << pathLossDb << " dB) for target SNR " << targetSnrDb << " dB. Clamping to 0.");
        pathLossDb = 0; 
    }

    Ptr<ConstantPropagationLossModel> propLoss = CreateObject<ConstantPropagationLossModel>();
    propLoss->SetConstant(pathLossDb);

    Ptr<YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel>();
    wifiChannel->AddPropagationLossModel(propLoss);

    // 4. Setup Wifi Phy
    YansWifiPhyHelper phyHelper;
    phyHelper.SetChannel(wifiChannel);
    phyHelper.Set("NoiseFigure", DoubleValue(noiseFigureDb)); // Set the receiver noise figure

    // Create and set the specified error rate model
    ObjectFactory errorModelFactory;
    errorModelFactory.SetTypeId(errorModelTypeId);
    Ptr<ErrorRateModel> errorModel = errorModelFactory.Create<ErrorRateModel>();
    phyHelper.SetErrorRateModel(errorModel);

    // 5. Setup Wifi Mac and NetDevices
    WifiMacHelper macHelper;
    Wifi80211nHelper wifi80211n; // For HT rates

    // Use AdhocWifiMac for simplicity (no association/beaconing overhead)
    macHelper.SetType("ns3::AdhocWifiMac",
                      "QosSupported", BooleanValue(true)); // Qos needed for HT modes
    NetDeviceContainer devices = wifi8011n.Install(phyHelper, macHelper, nodes);

    // Set TxPower for both devices' PHY layer
    Ptr<WifiNetDevice> device0 = DynamicCast<WifiNetDevice>(devices.Get(0));
    Ptr<WifiNetDevice> device1 = DynamicCast<WifiNetDevice>(devices.Get(1));
    device0->GetPhy()->SetTxPower(txPowerDb);
    device1->GetPhy()->SetTxPower(txPowerDb);

    // 6. Set Wifi Remote Station Manager to a specific MCS
    WifiRemoteStationManagerHelper remoteStationManager;
    remoteStationManager.SetAttribute("NonErp", BooleanValue(true)); // Required for 802.11n
    
    // Get the WifiMode for the given HT MCS (e.g., "HtMcs7")
    WifiMode wifiMode = WifiModeFactory::GetMcsMode(mcs);
    remoteStationManager.SetAttribute("DataMode", WifiModeValue(wifiMode));
    
    // Configure the remote station manager for both devices
    wifi80211n.ConfigureWifiRemoteStationManager(devices, &remoteStationManager);

    // 7. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 8. Setup Applications (OnOffApplication and PacketSink)
    uint16_t port = 9;

    // Packet Sink (receiver) on node 1
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simDuration));

    // OnOffApplication (sender) on node 0, sending to node 1
    OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.Get(1).GetLocal(), port));
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // Always on
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffHelper.SetAttribute("PacketSize", UintegerValue(frameSize));
    // Data rate needs to be significantly higher than the WiFi MCS rate to ensure the PHY is always saturated
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps"))); 
    
    ApplicationContainer onoffApps = onoffHelper.Install(nodes.Get(0));
    onoffApps.Start(Seconds(0.1)); // Start slightly after sink
    onoffApps.Stop(Seconds(simDuration));

    // 9. Run simulation
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    // 10. Collect statistics
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    Ptr<OnOffApplication> onoff = DynamicCast<OnOffApplication>(onoffApps.Get(0));

    uint64_t packetsReceived = sink->GetTotalRxPackets();
    uint64_t packetsSent = onoff->GetTotalTxPackets();

    double fsr = 0.0;
    if (packetsSent > 0)
    {
        fsr = static_cast<double>(packetsReceived) / packetsSent;
    }
    else
    {
        NS_LOG_WARN("No packets sent for MCS " << mcs << " at SNR " << targetSnrDb << " dB. FSR set to 0.");
    }
    
    NS_LOG_INFO("  -> Packets Sent: " << packetsSent << ", Received: " << packetsReceived << ", FSR: " << fsr);

    return fsr;
}


int main(int argc, char *argv[])
{
    // Enable logging for relevant components to debug if needed
    LogComponentEnable("WifiErrorRateModelValidation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_WARN);
    LogComponentEnable("YansWifiPhy", LOG_LEVEL_WARN);
    LogComponentEnable("WifiRemoteStationManager", LOG_LEVEL_WARN);

    // Command line arguments for configurable parameters
    CommandLine cmd(__FILE__);
    cmd.AddValue("simDuration", "Simulation duration in seconds", g_simDuration);
    cmd.AddValue("frameSize", "Application frame size in bytes", g_frameSize);
    cmd.AddValue("minSnrDb", "Minimum SNR to test in dB", g_minSnrDb);
    cmd.AddValue("maxSnrDb", "Maximum SNR to test in dB", g_maxSnrDb);
    cmd.AddValue("snrStepDb", "SNR step increment in dB", g_snrStepDb);
    cmd.AddValue("txPowerDb", "Transmit power in dBm", g_txPowerDb);
    cmd.AddValue("noiseFigureDb", "Noise figure in dB", g_noiseFigureDb);
    cmd.AddValue("maxMcs", "Maximum HT MCS index to test (0-7 for SISO, 0-15 for 2x2 MIMO, etc.)", g_maxMcs);
    cmd.Parse(argc, argv);

    // List of error models to compare
    std::vector<std::pair<std::string, TypeId>> errorModels = {
        {"Nist", NistErrorRateModel::GetTypeId()},
        {"Yans", YansErrorRateModel::GetTypeId()},
        {"TableBased", TableBasedErrorRateModel::GetTypeId()}
    };

    // Iterate over each HT MCS value
    for (int mcs = 0; mcs <= g_maxMcs; ++mcs)
    {
        // Get the WifiMode string for the current MCS for plot title/label
        WifiMode currentWifiMode = WifiModeFactory::GetMcsMode(mcs);
        std::string mcsDescription = currentWifiMode.GetModeName(); // e.g., "HtMcs7"

        // Define output filenames for Gnuplot script and plot image
        std::string plotFileName = "fsr_vs_snr_" + mcsDescription + ".plt";
        std::string outputFileName = "fsr_vs_snr_" + mcsDescription + ".png"; 

        // Create a Gnuplot object for the current MCS
        Gnuplot plot(outputFileName); 
        plot.SetTitle("Frame Success Rate vs. SNR for " + mcsDescription + " (Frame Size: " + std::to_string(g_frameSize) + " bytes)");
        plot.SetXLabel("SNR (dB)");
        plot.SetYLabel("Frame Success Rate");
        plot.AppendExtra("set grid");
        plot.AppendExtra("set yrange [0:1]"); // FSR is between 0 and 1

        // Iterate over each error rate model
        for (const auto& modelEntry : errorModels)
        {
            std::string modelName = modelEntry.first;
            TypeId modelTypeId = modelEntry.second;

            // Create a GnuplotDataset for the current error model
            GnuplotDataset dataset;
            dataset.SetTitle(modelName);
            dataset.SetStyle("linespoints"); // Connect points with lines and show points

            // Iterate over the range of SNR values
            for (double snrDb = g_minSnrDb; snrDb <= g_maxSnrDb; snrDb += g_snrStepDb)
            {
                // Run the simulation for the current configuration
                double fsr = RunSingleSimulation(mcs, snrDb, modelTypeId, 
                                                 g_txPowerDb, g_noiseFigureDb, g_thermalNoiseDb, 
                                                 g_frameSize, g_simDuration);
                // Add the data point to the dataset
                dataset.Add(snrDb, fsr);
            }
            // Add the dataset to the Gnuplot plot
            plot.AddDataset(dataset);
        }
        // Generate the Gnuplot script file
        std::ofstream plotFile(plotFileName.c_str());
        plot.GenerateOutput(plotFile);
        plotFile.close();
    }

    return 0;
}