#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/ssid.h"
#include "ns3/constant-rate-wifi-manager.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsThroughput");

// Global configurable parameters
double g_simTime = 10.0; // seconds
double g_distance = 10.0; // meters
std::string g_phyType = "Spectrum"; // "Spectrum" or "Yans"
std::string g_errorModelType = "None"; // "None", "Nist", "Pirofski"
uint32_t g_channelWidthMhz = 20; // Default channel width in MHz
std::string g_guardIntervalStr = "Short"; // "Short" (0.8us) or "Long" (3.2us)

/**
 * @brief Runs a single ns-3 Wi-Fi simulation with specified parameters.
 *
 * @param mcsIndex The Modulation and Coding Scheme index to test.
 * @param channelWidthMhz The channel width in MHz.
 * @param gi The Guard Interval (Short or Long).
 * @param wifiStandard The Wi-Fi standard to use (e.g., 802.11ax, 802.11n).
 * @param wifiModeString The specific Wi-Fi mode string (e.g., "HeMcs0", "HtMcs7").
 * @return The received data throughput in Mbps.
 */
double RunSingleSimulation(int mcsIndex, uint32_t channelWidthMhz, WifiGuardInterval gi,
                           WifiStandard wifiStandard, const std::string& wifiModeString)
{
    // Clear previous simulation state to ensure a clean run for each iteration
    Simulator::Destroy();

    NS_LOG_INFO("Running simulation for " << g_phyType << " PHY, Standard "
                << " (MCS=" << mcsIndex << ", Mode=" << wifiModeString
                << "), ChannelWidth=" << channelWidthMhz << "MHz, GI="
                << (gi == WIFI_GI_0_8_US ? "Short" : "Long"));

    // 1. Create nodes: one AP and one STA
    NodeContainer nodes;
    nodes.Create(2); 
    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // 2. Mobility: Constant position for both nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    
    Ptr<ConstantPositionMobilityModel> apMobility = CreateObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    apNode->AggregateObject(apMobility);

    Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel>();
    staMobility->SetPosition(Vector(g_distance, 0.0, 0.0));
    staNode->AggregateObject(staMobility);

    // 3. Install Internet stack (IPv4, TCP, UDP) on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Wi-Fi devices setup
    WifiHelper wifi;
    wifi.SetStandard(wifiStandard);

    // Configure channel and propagation model
    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue(3.0)); // Simple path loss model
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");

    // Dynamically create and configure the specific WifiPhyHelper based on user input
    Ptr<WifiPhyHelper> phyHelper;
    if (g_phyType == "Spectrum")
    {
        phyHelper = CreateObject<SpectrumWifiPhyHelper>();
    }
    else // Yans
    {
        phyHelper = CreateObject<YansWifiPhyHelper>();
    }
    phyHelper->SetChannel(channel.Create());

    // Configure error model if specified
    if (g_errorModelType == "Nist")
    {
        phyHelper->SetErrorRateModel("ns3::NistErrorRateModel");
    }
    else if (g_errorModelType == "Pirofski")
    {
        phyHelper->SetErrorRateModel("ns3::PirofskiErrorRateModel");
    }

    // Set Guard Interval for PHY (common attribute to both YansWifiPhy and SpectrumWifiPhy)
    phyHelper->Set("GuardInterval", EnumValue(gi));

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Use ConstantRateWifiManager to force the selected MCS for testing
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(wifiModeString),
                                 "ControlMode", StringValue(wifiModeString));

    // Install Wi-Fi devices based on the chosen standard
    // HeWifiHelper for 802.11ax, HtWifiHelper for 802.11n, or generic install for others
    if (wifiStandard == WIFI_STANDARD_80211ax)
    {
        HeWifiHelper heWifi;
        heWifi.SetChannelWidth(channelWidthMhz); // Applies to 802.11ax PHYs
        
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
        apDevices = heWifi.Install(*phyHelper, mac, apNode);

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices = heWifi.Install(*phyHelper, mac, staNode);
    }
    else if (wifiStandard == WIFI_STANDARD_80211n)
    {
        HtWifiHelper htWifi;
        htWifi.SetChannelWidth(channelWidthMhz); // Applies to 802.11n PHYs
        
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
        apDevices = htWifi.Install(*phyHelper, mac, apNode);

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices = htWifi.Install(*phyHelper, mac, staNode);
    }
    else // Fallback for 802.11a/b/g and other standards not specifically requiring HE/HT helpers
    {
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
        apDevices = wifi.Install(*phyHelper, mac, apNode);

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices = wifi.Install(*phyHelper, mac, staNode);
    }
    
    // 5. Assign IP addresses to the Wi-Fi devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(apDevices);
    interfaces.Add(ipv4.Assign(staDevices));

    // Get AP's IP address for the client to send data to
    Ipv4Address apIp = interfaces.GetAddress(0);

    // 6. Setup Applications (UDP Traffic from STA to AP)
    uint16_t port = 9; // Destination port for UDP packets

    // AP side: Packet Sink (server) to receive UDP packets
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::Any(), port));
    ApplicationContainer serverApps = sinkHelper.Install(apNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(g_simTime + 1.0)); // Stop server slightly after client

    // STA side: OnOff Application (client) to generate UDP traffic
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apIp, port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always on
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never off
    onoff.SetAttribute("PacketSize", UintegerValue(1472)); // Typical max payload for Ethernet MTU
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps"))); // Set a high data rate to not bottleneck Wi-Fi
    ApplicationContainer clientApps = onoff.Install(staNode);
    clientApps.Start(Seconds(1.0)); // Start client after 1 second to allow association
    clientApps.Stop(Seconds(g_simTime));

    // 7. Run the simulation
    Simulator::Run();

    // 8. Collect and return throughput statistics
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));
    uint64_t totalRxBytes = sink->GetTotalRx();
    double actualThroughputMbps = (totalRxBytes * 8.0) / (g_simTime * 1e6); // Calculate throughput in Mbps

    return actualThroughputMbps;
}

int main(int argc, char *argv[])
{
    // Enable logging for relevant components for debugging
    LogComponentEnable("WifiMcsThroughput", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("WifiMac", LOG_LEVEL_WARN);
    LogComponentEnable("WifiPhy", LOG_LEVEL_WARN);
    LogComponentEnable("ConstantRateWifiManager", LOG_LEVEL_WARN);
    LogComponentEnable("WifiRemoteStationManager", LOG_LEVEL_WARN);


    // Command line parameter parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time in seconds", g_simTime);
    cmd.AddValue("distance", "Distance between AP and STA in meters", g_distance);
    cmd.AddValue("phyType", "Wi-Fi PHY type (Spectrum or Yans)", g_phyType);
    cmd.AddValue("errorModelType", "Error model type (None, Nist, Pirofski)", g_errorModelType);
    cmd.AddValue("channelWidthMhz", "Channel width in MHz (e.g., 20, 40, 80, 160)", g_channelWidthMhz);
    cmd.AddValue("guardInterval", "Guard interval (Short or Long)", g_guardIntervalStr);
    cmd.Parse(argc, argv);

    // Validate command line parameters
    if (g_phyType != "Spectrum" && g_phyType != "Yans")
    {
        NS_LOG_ERROR("Invalid phyType. Must be 'Spectrum' or 'Yans'.");
        return 1;
    }
    if (g_errorModelType != "None" && g_errorModelType != "Nist" && g_errorModelType != "Pirofski")
    {
        NS_LOG_ERROR("Invalid errorModelType. Must be 'None', 'Nist', or 'Pirofski'.");
        return 1;
    }
    if (g_guardIntervalStr != "Short" && g_guardIntervalStr != "Long")
    {
        NS_LOG_ERROR("Invalid guardInterval. Must be 'Short' or 'Long'.");
        return 1;
    }

    // Convert guard interval string to enum
    WifiGuardInterval giEnum = (g_guardIntervalStr == "Short") ? WIFI_GI_0_8_US : WIFI_GI_3_2_US;

    // Define MCS indices, WifiStandard, and mode string prefix based on chosen PHY type
    std::vector<int> mcsIndices;
    WifiStandard selectedWifiStandard;
    std::string mcsPrefix;

    if (g_phyType == "Spectrum")
    {
        selectedWifiStandard = WIFI_STANDARD_80211ax;
        mcsIndices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; // 802.11ax MCS range (0-11)
        mcsPrefix = "HeMcs";
        // 802.11ax supports 20, 40, 80, 160 MHz channel widths natively.
        if (g_channelWidthMhz != 20 && g_channelWidthMhz != 40 && g_channelWidthMhz != 80 && g_channelWidthMhz != 160)
        {
            NS_LOG_WARN("Channel width " << g_channelWidthMhz << "MHz may not be fully standard-compliant for 802.11ax. Recommended: 20, 40, 80, 160.");
        }
    }
    else // Yans
    {
        // YansWifiPhy primarily supports 802.11a/b/g/n. For MCS and channel widths, 802.11n (HT) is the best match.
        selectedWifiStandard = WIFI_STANDARD_80211n;
        mcsIndices = {0, 1, 2, 3, 4, 5, 6, 7}; // 802.11n HT MCS range (0-7)
        mcsPrefix = "HtMcs";
        // 802.11n supports 20 or 40 MHz channel widths.
        if (g_channelWidthMhz > 40)
        {
            NS_LOG_WARN("Channel width " << g_channelWidthMhz << "MHz is not supported for 802.11n (YansWifiPhy). Falling back to 40MHz.");
            g_channelWidthMhz = 40; // Max for HT modes
        }
    }

    // Map for clear output of WifiStandard enum to string
    std::map<WifiStandard, std::string> standardMap = {
        {WIFI_STANDARD_80211a, "802.11a"},
        {WIFI_STANDARD_80211b, "802.11b"},
        {WIFI_STANDARD_80211g, "802.11g"},
        {WIFI_STANDARD_80211n, "802.11n"},
        {WIFI_STANDARD_80211ac, "802.11ac"},
        {WIFI_STANDARD_80211ax, "802.11ax"}
    };

    // Output header for results table
    std::cout << "PhyType\tStandard\tMCS Index\tMode String\tChannel Width (MHz)\tGuard Interval\tReceived Throughput (Mbps)\n";
    std::cout << "-----------------------------------------------------------------------------------------------------------------------\n";

    // Loop through each defined MCS index and run a separate simulation
    for (int mcs : mcsIndices)
    {
        std::string wifiModeString = mcsPrefix + std::to_string(mcs);
        double throughput = RunSingleSimulation(mcs, g_channelWidthMhz, giEnum, selectedWifiStandard, wifiModeString);
        
        // Print results for the current MCS
        std::cout << g_phyType << "\t"
                  << standardMap[selectedWifiStandard] << "\t"
                  << mcs << "\t\t"
                  << wifiModeString << "\t\t"
                  << g_channelWidthMhz << "\t\t\t"
                  << g_guardIntervalStr << "\t\t"
                  << std::fixed << std::setprecision(2) << throughput << "\n";
    }

    return 0;
}