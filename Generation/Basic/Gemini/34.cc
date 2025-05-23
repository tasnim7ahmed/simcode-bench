#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/ssid.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath> // For std::log10, std::pow
#include <sstream> // For std::ostringstream

// Using directives
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelValidation");

// Global counters for tracing PHY transmissions and receptions
uint32_t g_packetsTransmitted = 0;
uint32_t g_packetsReceived = 0;

void
TxCountCallback(Ptr<const Packet> packet)
{
    g_packetsTransmitted++;
}

void
RxCountCallback(Ptr<const Packet> packet, double snr, WifiMode mode, double rxPower)
{
    g_packetsReceived++;
}

/**
 * \brief Calculates the distance between nodes required to achieve a specific path loss
 *        using the Friis propagation loss model.
 *
 * The Friis propagation loss model is L_p [dB] = 20 log10(d [m]) + 20 log10(f [Hz]) + 20 log10(4*PI/c)
 * where 20 log10(4*PI/c) is approximately -147.5501 dB.
 *
 * \param pathLoss_dB The desired path loss in dB.
 * \param freq_GHz The operating frequency in GHz.
 * \return The distance in meters.
 */
double
CalculateDistance(double pathLoss_dB, double freq_GHz)
{
    double freq_Hz = freq_GHz * 1e9;
    // Friis model constant: 20 * log10(4 * M_PI / C_LIGHT_MPS) where C_LIGHT_MPS = 299792458.0
    // This evaluates to approximately -147.5501
    double friisConstant = -147.5501;
    // Rearranging the Friis equation to solve for distance (d):
    // pathLoss_dB = 20 * log10(d) + 20 * log10(freq_Hz) + friisConstant
    // 20 * log10(d) = pathLoss_dB - 20 * log10(freq_Hz) - friisConstant
    // log10(d) = (pathLoss_dB - 20 * log10(freq_Hz) - friisConstant) / 20.0
    double exponent = (pathLoss_dB - (20.0 * std::log10(freq_Hz)) - friisConstant) / 20.0;
    return std::pow(10.0, exponent);
}

/**
 * \brief Returns a vector of strings representing all standard HE_SU MCS modes (HeMcs0 to HeMcs11).
 * \return A vector of WifiMode strings.
 */
std::vector<std::string>
GetHeSuModes()
{
    std::vector<std::string> heSuModes;
    for (uint8_t mcs = 0; mcs <= 11; ++mcs)
    { // HE_SU MCS 0-11
        std::ostringstream oss;
        oss << "HeMcs" << static_cast<int>(mcs);
        heSuModes.push_back(oss.str());
    }
    return heSuModes;
}

/**
 * \brief A custom ns-3 Application to generate and send fixed-size packets via WifiMac.
 *
 * This application creates packets of a specified size and sends them at a fixed interval
 * through a provided WifiMac instance until a maximum number of packets is reached.
 */
class WifiPacketSource : public Application
{
public:
    static TypeId GetTypeId();
    WifiPacketSource();
    virtual ~WifiPacketSource();
    void SetTxMac(Ptr<WifiMac> mac);
    void SetPacketSize(uint32_t size);
    void SetInterval(Time interval);
    void SetMaxPackets(uint32_t maxPackets);

private:
    virtual void DoInitialize();
    virtual void DoStart();
    virtual void DoDispose();
    void SendPacket();

    uint32_t m_packetSize;   //!< Size of the packet payload (bytes)
    Time m_interval;         //!< Time interval between packet transmissions
    uint32_t m_maxPackets;   //!< Maximum number of packets to send
    uint32_t m_packetsSent;  //!< Counter for packets sent
    EventId m_sendEvent;     //!< Event ID for the next SendPacket event
    Ptr<WifiMac> m_txMac;    //!< Pointer to the WifiMac to use for transmission
};

TypeId
WifiPacketSource::GetTypeId()
{
    static TypeId tid = TypeId("ns3::WifiPacketSource")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<WifiPacketSource>()
        .AddAttribute("PacketSize", "The size of packets to send (MAC payload)",
                      UintegerValue(1024),
                      MakeUintegerAccessor(&WifiPacketSource::m_packetSize),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("Interval", "The time to wait between packets",
                      TimeValue(MicroSeconds(100)),
                      MakeTimeAccessor(&WifiPacketSource::m_interval),
                      MakeTimeChecker())
        .AddAttribute("MaxPackets", "The maximum number of packets to send",
                      UintegerValue(1000),
                      MakeUintegerAccessor(&WifiPacketSource::m_maxPackets),
                      MakeUintegerChecker<uint32_t>());
    return tid;
}

WifiPacketSource::WifiPacketSource()
    : m_packetSize(0), m_interval(MicroSeconds(0)), m_maxPackets(0),
      m_packetsSent(0), m_txMac(nullptr)
{
}

WifiPacketSource::~WifiPacketSource()
{
    // Cancel any pending events to avoid crashes during cleanup
    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }
}

void
WifiPacketSource::SetTxMac(Ptr<WifiMac> mac)
{
    m_txMac = mac;
}

void
WifiPacketSource::SetPacketSize(uint32_t size)
{
    m_packetSize = size;
}

void
WifiPacketSource::SetInterval(Time interval)
{
    m_interval = interval;
}

void
WifiPacketSource::SetMaxPackets(uint32_t maxPackets)
{
    m_maxPackets = maxPackets;
}

void
WifiPacketSource::DoInitialize()
{
    Application::DoInitialize();
}

void
WifiPacketSource::DoStart()
{
    m_packetsSent = 0;
    // Schedule the first packet transmission
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &WifiPacketSource::SendPacket, this);
}

void
WifiPacketSource::DoDispose()
{
    Application::DoDispose();
    m_txMac = nullptr; // Clear the pointer to avoid dangling references
}

void
WifiPacketSource::SendPacket()
{
    if (m_packetsSent < m_maxPackets)
    {
        if (m_txMac)
        {
            // Create a new packet with the specified payload size
            Ptr<Packet> packet = Create<Packet>(m_packetSize);

            // For simplicity, we send to broadcast address.
            // In a real AP-STA setup, this would typically be the unicast MAC address of the AP.
            // For PHY/MAC error model validation, broadcast is sufficient as long as it reaches the receiver.
            Mac48Address destMac = Mac48Address::GetBroadcast(); 
            
            // Send the packet via the WifiMac. This will add MAC headers and pass to PHY.
            // This implicitly causes the TxEnd trace on the PHY to be fired.
            m_txMac->SendData(packet, destMac);
            m_packetsSent++;
        }
        else
        {
            NS_LOG_WARN("WifiPacketSource: No WifiMac set. Cannot send packets.");
        }

        // Schedule the next packet transmission
        m_sendEvent = Simulator::Schedule(m_interval, &WifiPacketSource::SendPacket, this);
    }
    else
    {
        // All packets have been sent
        // NS_LOG_UNCOND("WifiPacketSource: All packets sent.");
    }
}

int
main(int argc, char* argv[])
{
    // Configure logging
    LogComponentEnable("HeErrorRateModelValidation", LOG_LEVEL_INFO);
    LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);
    LogComponentEnable("WifiMac", LOG_LEVEL_DEBUG); // For MAC SendData trace debug

    // Command-line arguments
    uint32_t frameSize = 1024; // Default MAC payload size in bytes
    std::string outputDir = "."; // Default output directory
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "MAC payload size in bytes", frameSize);
    cmd.AddValue("outputDir", "Directory to save Gnuplot data and plots", outputDir);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnableAll(LOG_LEVEL_INFO);
    }

    // Simulation parameters
    const double simulationTime = 1.0; // Seconds, needs to be long enough for all packets to be processed
    const uint32_t numPackets = 5000;  // Number of packets per (mode, SNR, model) combination
    const double minSnr = 0.0;         // Minimum SNR in dB
    const double maxSnr = 30.0;        // Maximum SNR in dB
    const double snrStep = 1.0;        // SNR step size in dB
    const double noiseFloor_dBm = -96.0; // Fixed noise floor in dBm
    const double txPower_dBm = 20.0;   // Transmit power in dBm (100 mW)
    const double freq_GHz = 5.180;     // Operating frequency (e.g., 802.11ax 20MHz channel 36)

    // Define error models to test
    std::vector<std::string> errorModelNames = {
        "ns3::NistErrorRateModel",
        "ns3::YansErrorRateModel",
        "ns3::TableBasedErrorRateModel"
    };

    // Get all HE_SU modes
    std::vector<std::string> heSuModes = GetHeSuModes();

    // Prepare Gnuplot objects for each error model
    std::map<std::string, Gnuplot> gnuplotObjects;
    // Map to store datasets for each model and mode combination
    std::map<std::string, Ptr<Gnuplot2dDataset>> gnuplotDatasets;

    // Initialize Gnuplot objects and their datasets
    for (const std::string& modelName : errorModelNames)
    {
        std::string plotFileName = outputDir + "/" + modelName.substr(5) + "_fsr_vs_snr.plt";
        gnuplotObjects.emplace(modelName, Gnuplot(plotFileName));
        Gnuplot& plot = gnuplotObjects.at(modelName);

        plot.SetTitle("Frame Success Rate vs. SNR for " + modelName.substr(5));
        plot.SetXLabel("SNR (dB)");
        plot.SetYLabel("Frame Success Rate");
        plot.AppendExtra("set terminal png size 1024,768");
        plot.AppendExtra("set output '" + outputDir + "/" + modelName.substr(5) + "_fsr_vs_snr.png'");
        plot.AppendExtra("set grid");
        plot.AppendExtra("set key outside");
        plot.AppendExtra("set xrange [" + std::to_string(minSnr) + ":" + std::to_string(maxSnr) + "]");
        plot.AppendExtra("set yrange [0:1.05]");

        for (const std::string& modeName : heSuModes)
        {
            Ptr<Gnuplot2dDataset> dataset = Create<Gnuplot2dDataset>();
            dataset->SetTitle(modeName);
            dataset->SetStyle("linespoints");
            plot.AddDataset(dataset);
            gnuplotDatasets.emplace(modelName + "_" + modeName, dataset);
        }
    }

    // Main simulation loops
    for (const std::string& modelName : errorModelNames)
    {
        NS_LOG_INFO("Simulating with error model: " << modelName);
        for (double snr_dB = minSnr; snr_dB <= maxSnr; snr_dB += snrStep)
        {
            // Calculate target RxPower based on SNR and NoiseFloor
            double rxPower_dBm = noiseFloor_dBm + snr_dB;
            // Calculate required path loss to achieve target RxPower
            double pathLoss_dB = txPower_dBm - rxPower_dBm;
            // Calculate distance to achieve required path loss
            double distance = CalculateDistance(pathLoss_dB, freq_GHz);

            NS_LOG_INFO("  SNR: " << snr_dB << " dB, Target RxPower: " << rxPower_dBm << " dBm, Distance: " << distance << " m");

            for (const std::string& modeName : heSuModes)
            {
                // Reset global counters for each individual simulation run
                g_packetsTransmitted = 0;
                g_packetsReceived = 0;

                // Create nodes
                NodeContainer nodes;
                nodes.Create(2); // Node 0: AP, Node 1: STA

                // Set mobility: AP at origin, STA at calculated distance
                Ptr<ConstantPositionMobilityModel> apMobility = CreateObject<ConstantPositionMobilityModel>();
                apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
                nodes.Get(0)->AggregateObject(apMobility);

                Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel>();
                staMobility->SetPosition(Vector(distance, 0.0, 0.0));
                nodes.Get(1)->AggregateObject(staMobility);

                // Configure Wifi for 802.11ax (HE)
                WifiHelper wifi;
                wifi.SetStandard(WIFI_STANDARD_80211ax);

                // Configure PHY layer
                YansWifiPhyHelper phyHelper;
                phyHelper.Set("PropagationLossModel", StringValue("ns3::FriisPropagationLossModel"));
                phyHelper.Set("Frequency", DoubleValue(freq_GHz * 1e9)); // Frequency in Hz for FriisPropagationLossModel
                phyHelper.Set("RxNoiseFigure", DoubleValue(0.0)); // Assume perfect receiver for simpler SNR calculation
                phyHelper.Set("RxNoiseFloor", DoubleValue(noiseFloor_dBm)); // Set fixed noise floor

                // Set the current error rate model dynamically
                phyHelper.SetErrorRateModel(modelName);

                // Set transmit power for both nodes
                phyHelper.Set("TxPowerStart", DoubleValue(txPower_dBm));
                phyHelper.Set("TxPowerEnd", DoubleValue(txPower_dBm));

                // Configure MAC layer
                WifiMacHelper macHelper;
                Ssid ssid = Ssid("ns3-he-validation");

                // AP configuration
                macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
                NetDeviceContainer apDevice = wifi.Install(phyHelper, macHelper, nodes.Get(0));

                // STA configuration
                macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
                NetDeviceContainer staDevice = wifi.Install(phyHelper, macHelper, nodes.Get(1));

                // Set HE (802.11ax) specific configurations
                // This applies to all nodes, but ensures the desired MCS and Preamble are used.
                Config::Set("/NodeList/*/DeviceList/*/Mac/HeConfiguration/DefaultHeMode", StringValue(modeName));
                Config::Set("/NodeList/*/DeviceList/*/Mac/HeConfiguration/DefaultPreamble", EnumValue(WifiPreamble::HE_SU));

                // Connect PHY traces to count transmitted and received packets
                Ptr<WifiPhy> txPhy = DynamicCast<WifiNetDevice>(staDevice.Get(0))->GetPhy(); // STA's PHY
                Ptr<WifiPhy> rxPhy = DynamicCast<WifiNetDevice>(apDevice.Get(0))->GetPhy(); // AP's PHY

                txPhy->TraceConnectWithoutContext("TxEnd", MakeCallback(&TxCountCallback));
                // RxOk provides the SNR and WifiMode context but we don't use it for counting specifically.
                rxPhy->TraceConnectWithoutContext("RxOk", MakeCallback(&RxCountCallback));

                // Install custom WifiPacketSource application on STA
                Ptr<WifiMac> staMac = DynamicCast<WifiNetDevice>(staDevice.Get(0))->GetMac();
                
                ApplicationContainer sourceApps;
                Ptr<WifiPacketSource> sourceApp = CreateObject<WifiPacketSource>();
                sourceApp->SetTxMac(staMac);
                sourceApp->SetPacketSize(frameSize);
                sourceApp->SetInterval(MicroSeconds(simulationTime * 1e6 / numPackets)); // Adjust interval for target simulation time
                sourceApp->SetMaxPackets(numPackets);
                nodes.Get(1)->AddApplication(sourceApp);
                sourceApp->Start(Seconds(0.0));
                sourceApp->Stop(Seconds(simulationTime));
                sourceApps.Add(sourceApp);

                // Install PacketSink (basic Application) on AP to receive packets
                PacketSinkHelper sink("ns3::WifiMac", Mac48Address::GetBroadcast()); // Listens for all MAC packets
                ApplicationContainer sinkApps = sink.Install(nodes.Get(0));
                sinkApps.Start(Seconds(0.0));
                sinkApps.Stop(Seconds(simulationTime));

                // Run simulation for this specific configuration
                Simulator::Stop(Seconds(simulationTime));
                Simulator::Run();
                Simulator::Destroy(); // Clean up all objects for the next simulation run

                // Calculate Frame Success Rate
                double fsr = (g_packetsTransmitted > 0) ? static_cast<double>(g_packetsReceived) / g_packetsTransmitted : 0.0;
                NS_LOG_INFO("    Mode: " << modeName << ", Transmitted: " << g_packetsTransmitted << ", Received: " << g_packetsReceived << ", FSR: " << fsr);

                // Add data point to the corresponding Gnuplot dataset
                gnuplotDatasets.at(modelName + "_" + modeName)->Add(snr_dB, fsr);
            }
        }
    }

    // Write Gnuplot data files and plot commands
    for (auto const& [modelName, plotObject] : gnuplotObjects)
    {
        plotObject.PlotDatasets();
    }

    NS_LOG_INFO("Simulation finished. Gnuplot data and plots generated in " << outputDir);

    return 0;
}