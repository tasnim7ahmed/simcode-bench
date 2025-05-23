#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot-helper.h"
#include <string>
#include <vector>
#include <map>

// Global variables to collect stats from trace sinks for the current simulation run
static uint32_t g_packetsRx;
static uint32_t g_packetsTx;

// Trace callback for successful PHY reception (including status to check for OK)
void PhyRxOkTrace(std::string context, ns3::Ptr<const ns3::Packet> packet, double snr, ns3::WifiPhyRxStatus status)
{
    if (status == ns3::WIFI_PHY_RX_OK)
    {
        g_packetsRx++;
    }
}

// Trace callback for PHY transmission attempt
void PhyTxTrace(std::string context, ns3::Ptr<const ns3::Packet> packet, ns3::WifiPhyTxVector txVector)
{
    g_packetsTx++;
}

/**
 * Runs a single simulation point to compute Frame Success Rate for a given
 * SNR, OFDM mode, and error rate model.
 *
 * @param snrDb Signal-to-Noise Ratio in dB.
 * @param ofdmMode The OFDM data rate mode (e.g., "OfdmRate6Mbps").
 * @param errorModelString The ns-3 error rate model type (e.g., "ns3::NistErrorRateModel").
 * @param payloadSize Application payload size in bytes.
 * @param numPackets Number of packets to attempt sending.
 * @return The Frame Success Rate (FSR) for the given parameters.
 */
double RunSimulationPoint(double snrDb, std::string ofdmMode, std::string errorModelString,
                          uint32_t payloadSize, uint32_t numPackets)
{
    // Reset global counters for this simulation run
    g_packetsRx = 0;
    g_packetsTx = 0;

    // Destroy any previous simulation components to ensure a clean state
    ns3::Simulator::Destroy();

    // 1. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Set up WiFi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211a); // Specify 802.11a for OFDM rates

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantPropagationDelayModel");
    // Set 0 dB loss as we are directly controlling SNR via SetSnrForTesting
    channel.AddPropagationLoss("ns3::ConstantPropagationLossModel", "Loss", ns3::DoubleValue(0.0));

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetErrorRateModel(errorModelString); // Set the specific error rate model for evaluation

    ns3::NqosWifiMacHelper mac = ns3::NqosWifiMacHelper::Default();
    // Use ConstantRateWifiManager to force the desired OFDM mode
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue(ofdmMode),
                                 "FragmentationThreshold", ns3::UintegerValue(2200), // Disable fragmentation
                                 "RtsCtsThreshold", ns3::UintegerValue(2200));       // Disable RTS/CTS

    ns3::NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    // Get a pointer to the actual WifiPhy object for setting SNR
    ns3::Ptr<ns3::WifiPhy> senderPhy = ns3::DynamicCast<ns3::WifiNetDevice>(devices.Get(0))->GetPhy();
    ns3::Ptr<ns3::WifiPhy> receiverPhy = ns3::DynamicCast<ns3::WifiNetDevice>(devices.Get(1))->GetPhy();

    // Set the desired SNR for testing. This directly influences the error model's calculation.
    receiverPhy->SetSnrForTesting(snrDb);

    // Connect traces for counting PHY packets
    senderPhy->TraceConnectWithoutContext("PhyTx", ns3::MakeCallback(&PhyTxTrace));
    receiverPhy->TraceConnectWithoutContext("PhyRxOk", ns3::MakeCallback(&PhyRxOkTrace));

    // 3. Install Internet Stack and Applications
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::Any, port));
    ns3::ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1)); // Receiver
    sinkApp.Start(ns3::Seconds(0.0));
    sinkApp.Stop(ns3::Seconds(100.0)); // Keep sink alive for the entire simulation duration

    ns3::OnOffHelper onoffHelper("ns3::UdpSocketFactory", ns3::InetSocketAddress(interfaces.GetAddress(1), port));
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(payloadSize));
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    
    // Set data rate to be less than the slowest OFDM mode (6Mbps) to avoid application-level congestion
    // The actual rate will be controlled by ConstantRateWifiManager at the PHY layer.
    onoffHelper.SetAttribute("DataRate", ns3::StringValue("5Mbps")); 
    onoffHelper.SetAttribute("MaxPackets", ns3::UintegerValue(numPackets));

    // Calculate required simulation time to send 'numPackets' at the specified OnOffApp data rate
    // Consider UDP (8 bytes) + IP (20 bytes) headers in addition to payload.
    double ipPacketSize = static_cast<double>(payloadSize + 28); // UDP (8) + IP (20) headers
    double requestedBits = ipPacketSize * 8.0 * static_cast<double>(numPackets);
    double onOffAppRateBps = 5e6; // 5 Mbps
    double requiredTime = requestedBits / onOffAppRateBps;

    ns3::ApplicationContainer onoffApp = onoffHelper.Install(nodes.Get(0)); // Sender
    onoffApp.Start(ns3::Seconds(0.0));
    onoffApp.Stop(ns3::Seconds(requiredTime + 0.1)); // Add a small buffer for app to stop

    // Run simulation slightly longer than the required time for packets to finish
    ns3::Simulator::Stop(ns3::Seconds(requiredTime + 0.2)); 
    ns3::Simulator::Run();

    double fsr = 0.0;
    if (g_packetsTx > 0)
    {
        fsr = static_cast<double>(g_packetsRx) / g_packetsTx;
    }
    else
    {
        NS_LOG_WARN("No packets transmitted for SNR " << snrDb << ", Mode " << ofdmMode << ", Model " << errorModelString);
    }
    
    return fsr;
}

int main(int argc, char *argv[])
{
    // Enable logging for relevant components for debugging (can be disabled for final runs)
    ns3::LogComponentEnable("YansWifiPhy", ns3::LOG_LEVEL_WARN); 
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_WARN);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_WARN);
    ns3::LogComponentEnable("ConstantRateWifiManager", ns3::LOG_LEVEL_WARN);

    // Default simulation parameters
    uint32_t payloadSize = 1000; // bytes
    uint32_t numPackets = 10000; // packets per SNR point for statistical robustness
    double snrStep = 1.0;        // SNR step size in dB

    // Command line argument parsing
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size of the packets in bytes", payloadSize);
    cmd.AddValue("numPackets", "Number of packets to send per SNR point", numPackets);
    cmd.AddValue("snrStep", "Step size for SNR in dB", snrStep);
    cmd.Parse(argc, argv);

    // List of 802.11a OFDM modes to test
    std::vector<std::string> ofdmModes = {
        "OfdmRate6Mbps", "OfdmRate9Mbps", "OfdmRate12Mbps", "OfdmRate18Mbps",
        "OfdmRate24Mbps", "OfdmRate36Mbps", "OfdmRate48Mbps", "OfdmRate54Mbps"
    };

    // List of error rate models to test
    std::vector<std::string> errorRateModels = {
        "ns3::NistErrorRateModel",
        "ns3::YansErrorRateModel",
        "ns3::TableBasedErrorRateModel"
    };

    // Main loop: Iterate through each error rate model
    for (const auto& currentErrorModel : errorRateModels)
    {
        ns3::GnuplotHelper plotHelper;
        plotHelper.SetTitle("Frame Success Rate vs. SNR for " + currentErrorModel);
        plotHelper.SetXAxisLabel("SNR (dB)");
        plotHelper.SetYAxisLabel("Frame Success Rate");
        plotHelper.AppendExtraPlot("set yrange [0:1]"); // FSR is bounded between 0 and 1
        plotHelper.AppendExtraPlot("set key autotitle columnhead"); // Use dataset title for legend

        // Generate a clean filename for the Gnuplot output
        std::string plotFileName = "fsr-vs-snr-" + currentErrorModel;
        // Remove "ns3::" prefix
        size_t ns3PrefixPos = plotFileName.find("ns3::");
        if (ns3PrefixPos != std::string::npos)
        {
            plotFileName = plotFileName.substr(ns3PrefixPos + 5);
        }
        // Remove "ErrorRateModel" suffix
        size_t errorModelSuffixPos = plotFileName.find("ErrorRateModel");
        if (errorModelSuffixPos != std::string::npos)
        {
            plotFileName.replace(errorModelSuffixPos, std::string("ErrorRateModel").length(), "");
        }
        plotFileName += ".plt"; // Add .plt extension

        // Loop: Iterate through each OFDM mode
        for (const auto& currentOfdmMode : ofdmModes)
        {
            ns3::Gnuplot2dDataset dataset;
            dataset.SetTitle(currentOfdmMode); // Dataset title for legend
            dataset.SetStyle(ns3::Gnuplot2dDataset::LINES_POINTS);

            // Loop: Iterate through SNR values from -5 dB to 30 dB
            for (double snrDb = -5.0; snrDb <= 30.0; snrDb += snrStep)
            {
                double fsr = RunSimulationPoint(snrDb, currentOfdmMode, currentErrorModel, payloadSize, numPackets);
                dataset.Add(snrDb, fsr);
            }
            plotHelper.AddDataset(dataset);
        }
        plotHelper.Output(plotFileName); // Save the plot for the current error model
    }

    return 0;
}