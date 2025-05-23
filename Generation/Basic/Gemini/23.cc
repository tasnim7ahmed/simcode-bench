#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <cmath> // For std::pow

// Use specific ns-3 namespaces
using namespace ns3;

// Log component for this script
NS_LOG_COMPONENT_DEFINE("EhtThroughputSimulation");

// Global variables for simulation parameters
static uint32_t g_numStations = 1;
static std::string g_trafficType = "UDP"; // "TCP" or "UDP"
static uint32_t g_payloadSize = 1472;      // Default for UDP, typical MTU for TCP
static uint32_t g_mpduBufferSize = 65535; // Default MPDU aggregation buffer size (bytes)
static double g_simulationTime = 10.0;    // seconds
static double g_throughputTolerance = 0.20; // 20% tolerance for validation

// Frequency and Band mapping
static uint32_t g_frequency = 5180; // Default: 5 GHz, Channel 36 (common for 5GHz)
static EhtPhyParameters::Band g_band = EhtPhyParameters::BAND_5GHZ;
static bool g_uplinkOfdma = false;
static bool g_bsrp = false;

std::string
GetChannelWidthString(EhtPhyParameters::ChannelWidth cw)
{
    switch (cw)
    {
    case EhtPhyParameters::CHANNEL_WIDTH_20MHZ:
        return "20MHz";
    case EhtPhyParameters::CHANNEL_WIDTH_40MHZ:
        return "40MHz";
    case EhtPhyParameters::CHANNEL_WIDTH_80MHZ:
        return "80MHz";
    case EhtPhyParameters::CHANNEL_WIDTH_160MHZ:
        return "160MHz";
    case EhtPhyParameters::CHANNEL_WIDTH_320MHZ:
        return "320MHz";
    default:
        return "UNKNOWN";
    }
}

std::string
GetGuardIntervalString(EhtPhyParameters::GuardInterval gi)
{
    switch (gi)
    {
    case EhtPhyParameters::GUARD_INTERVAL_NORMAL:
        return "Normal";
    case EhtPhyParameters::GUARD_INTERVAL_SHORT:
        return "Short";
    default:
        return "UNKNOWN";
    }
}

std::string
GetFrequencyBandString(EhtPhyParameters::Band band)
{
    switch (band)
    {
    case EhtPhyParameters::BAND_2_4GHZ:
        return "2.4 GHz";
    case EhtPhyParameters::BAND_5GHZ:
        return "5 GHz";
    case EhtPhyParameters::BAND_6GHZ:
        return "6 GHz";
    default:
        return "UNKNOWN";
    }
}

void
CalculateThroughput(Ptr<PacketSink> sink, double simTime,
                    uint8_t mcs, EhtPhyParameters::ChannelWidth cw, EhtPhyParameters::GuardInterval gi)
{
    uint64_t totalRxBytes = sink->GetTotalRx();
    double throughputMbps = (totalRxBytes * 8.0) / (simTime * 1000.0 * 1000.0);

    NS_LOG_INFO("  Calculated Throughput: " << throughputMbps << " Mbps");

    // Theoretical maximum throughput calculation (for Nss=1, approximate)
    // Ref: IEEE 802.11be draft / related sources for theoretical rates
    double bitsPerSymbolPerSubcarrier = 0.0;
    switch (mcs)
    {
    case 0: bitsPerSymbolPerSubcarrier = 0.5; break; // BPSK 1/2
    case 1: bitsPerSymbolPerSubcarrier = 1.0; break; // QPSK 1/2
    case 2: bitsPerSymbolPerSubcarrier = 1.5; break; // QPSK 3/4
    case 3: bitsPerSymbolPerSubcarrier = 2.0; break; // 16-QAM 1/2
    case 4: bitsPerSymbolPerSubcarrier = 3.0; break; // 16-QAM 3/4
    case 5: bitsPerSymbolPerSubcarrier = 4.0; break; // 64-QAM 2/3
    case 6: bitsPerSymbolPerSubcarrier = 4.5; break; // 64-QAM 3/4
    case 7: bitsPerSymbolPerSubcarrier = 5.0; break; // 64-QAM 5/6
    case 8: bitsPerSymbolPerSubcarrier = 6.0; break; // 256-QAM 3/4
    case 9: bitsPerSymbolPerSubcarrier = 6.666; break; // 256-QAM 5/6
    case 10: bitsPerSymbolPerSubcarrier = 7.5; break; // 1024-QAM 3/4
    case 11: bitsPerSymbolPerSubcarrier = 8.333; break; // 1024-QAM 5/6
    case 12: bitsPerSymbolPerSubcarrier = 9.0; break; // 4096-QAM 3/4
    case 13: bitsPerSymbolPerSubcarrier = 10.0; break; // 4096-QAM 5/6
    default: NS_FATAL_ERROR("Unknown MCS: " << (uint32_t)mcs); return;
    }

    uint32_t numDataSubcarriers = 0;
    switch (cw)
    {
    case EhtPhyParameters::CHANNEL_WIDTH_20MHZ: numDataSubcarriers = 52; break;
    case EhtPhyParameters::CHANNEL_WIDTH_40MHZ: numDataSubcarriers = 106; break;
    case EhtPhyParameters::CHANNEL_WIDTH_80MHZ: numDataSubcarriers = 242; break;
    case EhtPhyParameters::CHANNEL_WIDTH_160MHZ: numDataSubcarriers = 498; break;
    case EhtPhyParameters::CHANNEL_WIDTH_320MHZ: numDataSubcarriers = 1002; break;
    default: NS_FATAL_ERROR("Unknown Channel Width: " << GetChannelWidthString(cw)); return;
    }

    // OFDM symbol duration is 3.2 us for 802.11ax/be
    // Guard interval durations: Normal GI (0.8us), Short GI (0.4us)
    double symbolDurationUs = 3.2; // Base OFDM symbol duration
    if (gi == EhtPhyParameters::GUARD_INTERVAL_NORMAL)
    {
        symbolDurationUs += 0.8; // Total 4.0 us
    }
    else if (gi == EhtPhyParameters::GUARD_INTERVAL_SHORT)
    {
        symbolDurationUs += 0.4; // Total 3.6 us
    }
    else
    {
        NS_FATAL_ERROR("Unknown Guard Interval: " << GetGuardIntervalString(gi)); return;
    }

    // Assuming 1 spatial stream (Nss=1)
    double rawDataRateMbps = (bitsPerSymbolPerSubcarrier * numDataSubcarriers) / symbolDurationUs;

    // Account for MAC/PHY overhead and higher-layer protocol efficiency
    double efficiencyFactor = 0.85; // Typical for UDP
    if (g_trafficType == "TCP")
    {
        efficiencyFactor = 0.75; // TCP has more overheads and flow control
    }
    double theoreticalExpectedMbps = rawDataRateMbps * efficiencyFactor;

    double expectedMinMbps = theoreticalExpectedMbps * (1.0 - g_throughputTolerance);
    double expectedMaxMbps = theoreticalExpectedMbps * (1.0 + g_throughputTolerance);

    NS_LOG_INFO("  Expected Throughput Range: [" << expectedMinMbps << ", " << expectedMaxMbps << "] Mbps (Theoretical-Raw: " << rawDataRateMbps << " Mbps)");

    if (throughputMbps < expectedMinMbps || throughputMbps > expectedMaxMbps)
    {
        NS_FATAL_ERROR("  ERROR: Throughput (" << throughputMbps << " Mbps) is outside expected range ["
                                               << expectedMinMbps << ", " << expectedMaxMbps << "] Mbps for MCS="
                                               << (uint32_t)mcs << ", CW=" << GetChannelWidthString(cw)
                                               << ", GI=" << GetGuardIntervalString(gi) << ". Simulation terminated.");
    }
}

int
main(int argc, char* argv[])
{
    // Set default log level (optional, good for debugging)
    LogComponentEnable("EhtThroughputSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Wifi80211beHelper", LOG_LEVEL_INFO);
    LogComponentEnable("EhtPhy", LOG_LEVEL_INFO);
    LogComponentEnable("EhtMac", LOG_LEVEL_INFO);
    LogComponentEnable("StaWifiMac", LOG_LEVEL_INFO);
    LogComponentEnable("ApWifiMac", LOG_LEVEL_INFO);

    // Command-line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("numStations", "Number of Wi-Fi stations", g_numStations);
    cmd.AddValue("trafficType", "Traffic type (TCP or UDP)", g_trafficType);
    cmd.AddValue("payloadSize", "Payload size in bytes", g_payloadSize);
    cmd.AddValue("mpduBufferSize", "MPDU aggregation buffer size in bytes", g_mpduBufferSize);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", g_simulationTime);
    cmd.AddValue("frequency", "Operating frequency in MHz (e.g., 2412, 5180, 5955)", g_frequency);
    cmd.AddValue("uplinkOfdma", "Enable Uplink OFDMA (true/false)", g_uplinkOfdma);
    cmd.AddValue("bsrp", "Enable BSRP (Bandwidth Sharing Report Protocol) (true/false)", g_bsrp);
    cmd.AddValue("throughputTolerance", "Tolerance for throughput validation (e.g., 0.20 for 20%)", g_throughputTolerance);
    cmd.Parse(argc, argv);

    // Validate traffic type
    if (g_trafficType != "TCP" && g_trafficType != "UDP")
    {
        NS_FATAL_ERROR("Invalid trafficType. Must be 'TCP' or 'UDP'.");
    }

    // Determine band from frequency
    if (g_frequency >= 2400 && g_frequency <= 2500)
    {
        g_band = EhtPhyParameters::BAND_2_4GHZ;
    }
    else if (g_frequency >= 5000 && g_frequency <= 5900)
    {
        g_band = EhtPhyParameters::BAND_5GHZ;
    }
    else if (g_frequency >= 5925 && g_frequency <= 7125) // 6 GHz band typically 5.925-7.125 GHz
    {
        g_band = EhtPhyParameters::BAND_6GHZ;
    }
    else
    {
        NS_FATAL_ERROR("Unsupported frequency: " << g_frequency << " MHz. Please provide a common Wi-Fi frequency.");
    }

    // Define lists for iteration
    const std::vector<uint8_t> mcsValues = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
    };
    const std::vector<EhtPhyParameters::ChannelWidth> channelWidths = {
        EhtPhyParameters::CHANNEL_WIDTH_20MHZ,
        EhtPhyParameters::CHANNEL_WIDTH_40MHZ,
        EhtPhyParameters::CHANNEL_WIDTH_80MHZ,
        EhtPhyParameters::CHANNEL_WIDTH_160MHZ,
        EhtPhyParameters::CHANNEL_WIDTH_320MHZ
    };
    const std::vector<EhtPhyParameters::GuardInterval> guardIntervals = {
        EhtPhyParameters::GUARD_INTERVAL_NORMAL,
        EhtPhyParameters::GUARD_INTERVAL_SHORT
    };

    // Print header for the results table
    std::cout << std::left << std::setw(10) << "Freq (GHz)"
              << std::setw(6) << "MCS"
              << std::setw(10) << "BW"
              << std::setw(10) << "GI"
              << std::setw(12) << "Traffic"
              << std::setw(8) << "Stations"
              << std::setw(15) << "Throughput (Mbps)" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (uint8_t mcs : mcsValues)
    {
        for (EhtPhyParameters::ChannelWidth cw : channelWidths)
        {
            for (EhtPhyParameters::GuardInterval gi : guardIntervals)
            {
                // Reset ns-3 state for each simulation run within the loop
                Simulator::Destroy();
                // Ensure stop time is reset for each new simulation
                Simulator::SetStopTime(Seconds(0.0));

                NS_LOG_INFO("Running simulation for: Freq=" << GetFrequencyBandString(g_band)
                                                             << ", MCS=" << (uint32_t)mcs
                                                             << ", BW=" << GetChannelWidthString(cw)
                                                             << ", GI=" << GetGuardIntervalString(gi));

                // 1. Create nodes
                NodeContainer wifiStaNodes;
                wifiStaNodes.Create(g_numStations);
                NodeContainer wifiApNode;
                wifiApNode.Create(1);

                // 2. Create Wi-Fi channel
                Ptr<YansWifiChannel> channel = YansWifiChannelHelper::Create();
                channel->SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
                channel->SetPropagationLossModel("ns3::FriisPropagationLossModel");

                // 3. Create Wi-Fi PHY and MAC helpers
                YansWifiPhyHelper phy;
                phy.SetChannel(channel);
                phy.SetErrorRateModel("ns3::YansErrorRateModel");

                WifiMacHelper mac;
                Wifi80211beHelper wifi80211be = Wifi80211beHelper::Default();

                // 4. Configure 802.11be PHY parameters
                EhtPhyParameters ehtPhyParams;
                ehtPhyParams.SetMcs(mcs);
                ehtPhyParams.SetChannelWidth(cw);
                ehtPhyParams.SetGuardInterval(gi);
                ehtPhyParams.SetFrequency(g_frequency);
                ehtPhyParams.SetBand(g_band);
                // Other parameters like Nss, Dcm, RuAllocationMode are default or can be set here.
                // ehtPhyParams.SetNss(1); // Assuming 1 spatial stream for throughput calculation

                wifi80211be.SetPhyParameters(ehtPhyParams);

                // 5. Configure 802.11be MAC parameters
                EhtMacParameters ehtMacParams;
                ehtMacParams.SetUplinkOfdma(g_uplinkOfdma);
                ehtMacParams.SetBsrp(g_bsrp);
                ehtMacParams.SetMpduAggregationMaxByte(g_mpduBufferSize);
                // Other parameters like DlMimo, TfMacHdrDuration, MultiUser are default or can be set here.

                wifi80211be.SetMacParameters(ehtMacParams);

                // 6. Install Wi-Fi devices
                Ssid ssid = Ssid("ns3-wifi-eht");

                NetDeviceContainer staDevices;
                mac.SetType("ns3::StaWifiMac",
                            "Ssid", SsidValue(ssid),
                            "ActiveProbing", BooleanValue(false));
                staDevices = wifi80211be.Install(phy, mac, wifiStaNodes);

                NetDeviceContainer apDevices;
                mac.SetType("ns3::ApWifiMac",
                            "Ssid", SsidValue(ssid),
                            "QosSupported", BooleanValue(true));
                apDevices = wifi80211be.Install(phy, mac, wifiApNode);

                // 7. Configure mobility
                MobilityHelper mobility;
                mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                              "MinX", DoubleValue(0.0),
                                              "MinY", DoubleValue(0.0),
                                              "DeltaX", DoubleValue(5.0),
                                              "DeltaY", DoubleValue(5.0),
                                              "GridWidth", UintegerValue(3),
                                              "LayoutType", StringValue("RowFirst"));
                mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                mobility.Install(wifiApNode);
                mobility.Install(wifiStaNodes);

                // 8. Install Internet Stack
                InternetStackHelper stack;
                stack.Install(wifiApNode);
                stack.Install(wifiStaNodes);

                // 9. Assign IP Addresses
                Ipv4AddressHelper address;
                address.SetBase("10.0.0.0", "255.0.0.0");
                Ipv4InterfaceContainer apInterface;
                apInterface = address.Assign(apDevices);
                Ipv4InterfaceContainer staInterfaces;
                staInterfaces = address.Assign(staDevices);

                // 10. Setup traffic (AP as source, Stations as sinks for downlink throughput)
                ApplicationContainer serverApps;
                ApplicationContainer clientApps;

                uint16_t port = 9; // Discard port

                // Create a PacketSink on each STA
                for (uint32_t i = 0; i < g_numStations; ++i)
                {
                    PacketSinkHelper sinkHelper(g_trafficType == "TCP" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                                                InetSocketAddress(Ipv4Address::GetAny(), port));
                    serverApps.Add(sinkHelper.Install(wifiStaNodes.Get(i)));
                }

                // Create OnOff application on AP to send traffic to each STA
                for (uint32_t i = 0; i < g_numStations; ++i)
                {
                    OnOffHelper onoff(g_trafficType == "TCP" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                                      InetSocketAddress(staInterfaces.Get(i).GetLocal(), port));
                    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                    onoff.SetAttribute("PacketSize", UintegerValue(g_payloadSize));
                    // Set a very high datarate to ensure the Wi-Fi link is the bottleneck
                    DataRate onoffDataRate(1000 * 1000 * 1000); // 1 Gbps per station to saturate link
                    onoff.SetAttribute("DataRate", DataRateValue(onoffDataRate));
                    clientApps.Add(onoff.Install(wifiApNode.Get(0)));
                }

                serverApps.Start(Seconds(1.0)); // Start sinks before clients
                serverApps.Stop(Seconds(g_simulationTime + 1.0)); // Stop sinks when simulation ends

                clientApps.Start(Seconds(1.0)); // Start clients after 1 second
                clientApps.Stop(Seconds(g_simulationTime)); // Stop clients at end of simulation

                // 11. Run simulation
                Simulator::Stop(Seconds(g_simulationTime + 1.1)); // Ensure all packets are processed
                Simulator::Run();

                // 12. Calculate and Validate Aggregate Throughput
                uint64_t totalAllStationsRxBytes = 0;
                for (uint32_t i = 0; i < g_numStations; ++i)
                {
                    Ptr<PacketSink> currentSink = DynamicCast<PacketSink>(serverApps.Get(i));
                    totalAllStationsRxBytes += currentSink->GetTotalRx();
                }

                // Create a dummy sink for the CalculateThroughput function,
                // as it only needs total bytes from the sink for its logic.
                Ptr<PacketSink> aggregateSink = CreateObject<PacketSink>();
                aggregateSink->SetTotalRx(totalAllStationsRxBytes);

                double currentAggregateThroughputMbps = (totalAllStationsRxBytes * 8.0) / (g_simulationTime * 1000.0 * 1000.0);

                // Call the validation function with the aggregate sink for combined validation
                CalculateThroughput(aggregateSink, g_simulationTime, mcs, cw, gi);

                // Print results to table
                std::cout << std::left << std::setw(10) << GetFrequencyBandString(g_band)
                          << std::setw(6) << (uint32_t)mcs
                          << std::setw(10) << GetChannelWidthString(cw)
                          << std::setw(10) << GetGuardIntervalString(gi)
                          << std::setw(12) << g_trafficType
                          << std::setw(8) << g_numStations
                          << std::fixed << std::setprecision(3) << std::setw(15) << currentAggregateThroughputMbps << std::endl;
            }
        }
    }

    Simulator::Destroy();
    return 0;
}