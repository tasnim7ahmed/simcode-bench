#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/packet-sink.h"

#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // For std::fixed, std::setprecision

// Bring in ns3 namespaces
using namespace ns3;

// Global simulation parameters (to be configured by command line or nested loops)
enum TrafficType { UDP, TCP };
enum GuardInterval { SHORT_GI, LONG_GI };
// Enum to represent user's channel width selection, including the specific "80+80 MHz"
enum WifiStandardSelection { WIFI_STANDARD_SELECTION_20, WIFI_STANDARD_SELECTION_40, WIFI_STANDARD_SELECTION_80, WIFI_STANDARD_SELECTION_160, WIFI_STANDARD_SELECTION_80_PLUS_80 };

static bool g_rtsCts = false;
static TrafficType g_trafficType = UDP;
static GuardInterval g_guardInterval = LONG_GI;
static int g_mcs = 7; // MCS index (0-9 for 802.11ac)
static WifiStandardSelection g_channelWidthSelection = WIFI_STANDARD_SELECTION_80; // Default user selection
static double g_distance = 10.0; // Distance between STA and AP in meters
static uint32_t g_packetSize = 1472; // Application packet size in bytes (e.g., 1500 - IP/UDP headers)
static double g_simulationTime = 1.0; // Simulation time in seconds

/**
 * @brief Converts the user's WifiStandardSelection enum to the ns-3 internal WifiPhyStandard enum.
 *        Note: 80+80 MHz is logically a 160 MHz channel, with the distinction handled by VHT capabilities.
 * @param standard The user's selected channel width.
 * @return The corresponding ns3::WifiPhyStandard value.
 */
WifiPhyStandard GetWifiPhyStandard(WifiStandardSelection standard)
{
    switch (standard)
    {
    case WIFI_STANDARD_SELECTION_20: return WIFI_PHY_STANDARD_20_MHZ;
    case WIFI_STANDARD_SELECTION_40: return WIFI_PHY_STANDARD_40_MHZ;
    case WIFI_STANDARD_SELECTION_80: return WIFI_PHY_STANDARD_80_MHZ;
    case WIFI_STANDARD_SELECTION_160:
    case WIFI_STANDARD_SELECTION_80_PLUS_80: 
        return WIFI_PHY_STANDARD_160_MHZ; // Both 160MHz and 80+80MHz map to 160MHz physical channel
    default: return WIFI_PHY_STANDARD_80_MHZ; // Fallback
    }
}

/**
 * @brief Runs a single ns-3 simulation with the current global parameter settings.
 * @param goodputMbps Reference to store the calculated goodput in Mbps.
 */
void RunSimulation(double& goodputMbps)
{
    // Reset any configurations that might persist from previous runs to ensure clean slate
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999")); // Reset RTS/CTS
    LogComponentDisable("WifiPhy", LOG_LEVEL_ALL); // Ensure logging is off by default for clean output
    LogComponentDisable("VhtWifiMac", LOG_LEVEL_ALL);
    LogComponentDisable("PacketSink", LOG_LEVEL_ALL);
    LogComponentDisable("OnOffApplication", LOG_LEVEL_ALL);
    LogComponentDisable("BulkSendApplication", LOG_LEVEL_ALL);

    // 1. Create nodes: One station (STA) and one access point (AP)
    NodeContainer nodes;
    nodes.Create(2); 
    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // 2. Mobility: Set fixed positions for STA and AP
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);

    Ptr<ConstantPositionMobilityModel> apMobility = apNode->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> staMobility = staNode->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0)); // AP at origin
    staMobility->SetPosition(Vector(g_distance, 0.0, 0.0)); // STA at specified distance

    // 3. WiFi Setup: IEEE 802.11ac configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

    // 3.1. SpectrumWifiPhyHelper: Configure PHY layer for 802.11ac
    SpectrumWifiPhyHelper phy;
    phy.Set("TxPowerStart", DoubleValue(16.0)); // Transmit power in dBm
    phy.Set("TxPowerEnd", DoubleValue(16.0));
    phy.Set("TxPowerLevels", UintegerValue(1));
    phy.Set("Antennas", UintegerValue(2)); // Use 2x2 MIMO (can be customized)
    phy.Set("ActivePhyRxSensitivity", StringValue("-82.0")); // Receiver sensitivity in dBm

    // Channel and propagation model
    YansWifiChannelHelper channel;
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel"); // Friis propagation loss model
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel"); // Constant speed propagation delay
    phy.SetChannel(channel.Create());

    // Set the operating channel width based on user's selection
    phy.SetChannelWidth(GetWifiPhyStandard(g_channelWidthSelection));

    // 3.2. VhtWifiMacHelper: Configure MAC layer for 802.11ac
    VhtWifiMacHelper mac;
    // Install AP MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("my-ssid")),
                "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Install STA MAC
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("my-ssid")));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    // Get the VhtWifiMac objects to configure VHT Capabilities directly
    Ptr<VhtWifiMac> apVhtMac = DynamicCast<VhtWifiMac>(apDevices.Get(0)->GetMac());
    Ptr<VhtWifiMac> staVhtMac = DynamicCast<VhtWifiMac>(staDevices.Get(0)->GetMac());

    Ptr<VhtCapabilities> apVhtCaps = apVhtMac->GetVhtCapabilities();
    Ptr<VhtCapabilities> staVhtCaps = staVhtMac->GetVhtCapabilities();

    // Configure VHT Capabilities: Enable 80+80 MHz and 160 MHz capabilities for the device
    // regardless of the current operating channel width. This defines what the device *can* support.
    apVhtCaps->SetChannelWidthSet(WIFI_VHT_CAPABILITY_CHANNEL_WIDTH_80_PLUS_80_MHZ | WIFI_VHT_CAPABILITY_CHANNEL_WIDTH_160_MHZ);
    staVhtCaps->SetChannelWidthSet(WIFI_VHT_CAPABILITY_CHANNEL_WIDTH_80_PLUS_80_MHZ | WIFI_VHT_CAPABILITY_CHANNEL_WIDTH_160_MHZ);

    // Guard Interval configuration
    if (g_guardInterval == SHORT_GI)
    {
        apVhtCaps->SetShortGi80MHz(true);
        apVhtCaps->SetShortGi160MHz(true);
        staVhtCaps->SetShortGi80MHz(true);
        staVhtCaps->SetShortGi160MHz(true);
    }
    else
    {
        apVhtCaps->SetShortGi80MHz(false);
        apVhtCaps->SetShortGi160MHz(false);
        staVhtCaps->SetShortGi80MHz(false);
        staVhtCaps->SetShortGi160MHz(false);
    }

    // Supported MCS and NSS (MCS 0-9 for 1 spatial stream, as requested)
    VhtMcsAndNss apVhtMcsNss;
    apVhtMcsNss.SetMcsRange(0, g_mcs); // Allow up to selected MCS for NSS 1
    apVhtMcsNss.SetNss(1); // 1 spatial stream
    apVhtCaps->SetSupportedVhtMcsAndNss(apVhtMcsNss);

    VhtMcsAndNss staVhtMcsNss;
    staVhtMcsNss.SetMcsRange(0, g_mcs); // Allow up to selected MCS for NSS 1
    staVhtMcsNss.SetNss(1); // 1 spatial stream
    staVhtCaps->SetSupportedVhtMcsAndNss(staVhtMcsNss);

    // RTS/CTS threshold: "0" means always use, "999999" means never use (disable)
    if (g_rtsCts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
    }
    else
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
    }

    // 4. IP Stack: Install Internet stack on nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);

    // 5. Applications: Set up traffic (UDP or TCP)
    Ptr<PacketSink> localSink = nullptr; // To collect received bytes
    if (g_trafficType == UDP)
    {
        // UDP Client (STA to AP)
        OnOffHelper client("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), 9));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always on
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("PacketSize", UintegerValue(g_packetSize));
        client.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps"))); // Sufficiently high rate to saturate the link
        ApplicationContainer clientApps = client.Install(staNode);
        clientApps.Start(Seconds(0.0));
        clientApps.Stop(Seconds(g_simulationTime));

        // UDP Server (AP)
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        ApplicationContainer serverApps = sinkHelper.Install(apNode);
        localSink = DynamicCast<PacketSink>(serverApps.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(g_simulationTime));
    }
    else // TCP
    {
        // TCP Server (AP)
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        ApplicationContainer serverApps = sinkHelper.Install(apNode);
        localSink = DynamicCast<PacketSink>(serverApps.Get(0));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(g_simulationTime));

        // TCP Client (STA to AP)
        BulkSendHelper client("ns3::TcpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), 9));
        client.SetAttribute("MaxBytes", UintegerValue(0)); // Send until application stops (unlimited)
        client.SetAttribute("SendSize", UintegerValue(g_packetSize));
        ApplicationContainer clientApps = client.Install(staNode);
        clientApps.Start(Seconds(0.0));
        clientApps.Stop(Seconds(g_simulationTime));
    }

    // 6. Run simulation
    Simulator::Stop(Seconds(g_simulationTime));
    Simulator::Run();

    // 7. Calculate goodput: Total received bytes * 8 (bits/byte) / simulation time / 1,000,000 (for Mbps)
    goodputMbps = (double)localSink->GetTotalRxBytes() * 8.0 / (g_simulationTime * 1000000.0);

    // 8. Cleanup for next iteration: Frees all allocated memory and objects
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // Configure command line arguments for individual runs (useful for debugging specific cases)
    CommandLine cmd(__FILE__);
    cmd.AddValue("rtsCts", "Enable/disable RTS/CTS (0=disable, 1=enable)", g_rtsCts);
    cmd.AddValue("trafficType", "Traffic type (UDP=0, TCP=1)", (uint32_t&)g_trafficType);
    cmd.AddValue("guardInterval", "Guard Interval (LONG_GI=0, SHORT_GI=1)", (uint32_t&)g_guardInterval);
    cmd.AddValue("mcs", "MCS index (0-9)", g_mcs);
    cmd.AddValue("channelWidth", "Channel Width (20=0, 40=1, 80=2, 160=3, 80+80=4)", (uint32_t&)g_channelWidthSelection);
    cmd.AddValue("distance", "Distance between STA and AP in meters", g_distance);
    cmd.AddValue("packetSize", "Application packet size in bytes", g_packetSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", g_simulationTime);
    cmd.Parse(argc, argv);

    // Validate MCS input
    if (g_mcs < 0 || g_mcs > 9)
    {
        NS_FATAL_ERROR("MCS must be between 0 and 9.");
    }

    // Define parameter ranges for iterating over all combinations
    std::vector<bool> rtsCtsOptions = {false, true};
    std::vector<TrafficType> trafficTypeOptions = {UDP, TCP};
    std::vector<GuardInterval> giOptions = {LONG_GI, SHORT_GI};
    std::vector<int> mcsOptions = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<WifiStandardSelection> channelWidthOptions = {
        WIFI_STANDARD_SELECTION_20,
        WIFI_STANDARD_SELECTION_40,
        WIFI_STANDARD_SELECTION_80,
        WIFI_STANDARD_SELECTION_160,
        WIFI_STANDARD_SELECTION_80_PLUS_80
    };
    std::vector<double> distanceOptions = {1.0, 5.0, 10.0, 20.0, 30.0}; // Example distances to test

    // Output header for the results table
    std::cout << "RTS/CTS\tTraffic\tGI\tMCS\tChWidth\tDistance(m)\tGoodput(Mbps)" << std::endl;

    // Outer loop for iterating through all combinations of parameters
    for (bool rtsCts : rtsCtsOptions)
    {
        g_rtsCts = rtsCts;
        for (TrafficType trafficType : trafficTypeOptions)
        {
            g_trafficType = trafficType;
            for (GuardInterval gi : giOptions)
            {
                g_guardInterval = gi;
                for (int mcs : mcsOptions)
                {
                    g_mcs = mcs;
                    for (WifiStandardSelection chWidth : channelWidthOptions)
                    {
                        g_channelWidthSelection = chWidth;
                        for (double dist : distanceOptions)
                        {
                            g_distance = dist;

                            double goodput = 0.0;
                            // Run the simulation for the current set of parameters
                            RunSimulation(goodput);

                            // Output results for the current simulation
                            std::cout << (g_rtsCts ? "Enabled" : "Disabled") << "\t";
                            std::cout << (g_trafficType == UDP ? "UDP" : "TCP") << "\t";
                            std::cout << (g_guardInterval == SHORT_GI ? "Short" : "Long") << "\t";
                            std::cout << g_mcs << "\t";
                            
                            // Convert WifiStandardSelection enum to a readable string
                            std::string chWidthStr;
                            switch(g_channelWidthSelection) {
                                case WIFI_STANDARD_SELECTION_20: chWidthStr = "20MHz"; break;
                                case WIFI_STANDARD_SELECTION_40: chWidthStr = "40MHz"; break;
                                case WIFI_STANDARD_SELECTION_80: chWidthStr = "80MHz"; break;
                                case WIFI_STANDARD_SELECTION_160: chWidthStr = "160MHz"; break;
                                case WIFI_STANDARD_SELECTION_80_PLUS_80: chWidthStr = "80+80MHz"; break;
                                default: chWidthStr = "Unknown"; break;
                            }
                            std::cout << chWidthStr << "\t";
                            std::cout << std::fixed << std::setprecision(1) << g_distance << "\t\t"; // Distance with 1 decimal place
                            std::cout << std::fixed << std::setprecision(3) << goodput << std::endl; // Goodput with 3 decimal places
                        }
                    }
                }
            }
        }
    }

    return 0;
}