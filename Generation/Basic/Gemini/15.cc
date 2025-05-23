#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot-helper.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm> // For std::min/max

// Use ns3 namespace
using namespace ns3;

// Global parameters
enum TransportProtocol { UDP, TCP };
enum FrequencyBand { BAND_2_4_GHZ, BAND_5_0_GHZ };
enum ChannelWidth { BW_20_MHZ, BW_40_MHZ };

NS_LOG_COMPONENT_DEFINE("HtMimoSimulation");

TransportProtocol g_transportProtocol = TCP;
FrequencyBand g_frequencyBand = BAND_5_0_GHZ;
ChannelWidth g_channelWidth = BW_40_MHZ;
bool g_shortGuardInterval = true;
bool g_preambleDetection = true; // ns-3 YansWifiPhy has EnablePreambleDetection

double g_minDistance = 1.0;
double g_maxDistance = 50.0; // Max distance for simulation
double g_distanceStep = 5.0; // Distance step size
double g_simulationTime = 5.0; // Simulation time per run (seconds)

uint32_t g_packetSize = 1024; // Bytes

int g_maxMimoStreams = 4; // Max MIMO streams to simulate (1 to 4)
int g_specificMcsIndex = -1; // If -1, simulate all MCS 0-7, otherwise simulate only this MCS

std::string g_plotFileNamePrefix = "throughput_mimo_distance";

// Helper function to calculate throughput
double CalcThroughput(Ptr<PacketSink> sink, double simulationTime)
{
    // Total bytes received by the sink
    uint64_t totalBytesRx = sink->GetTotalRxBytes();
    // Calculate throughput in bits per second
    double throughputBps = (double)totalBytesRx * 8.0 / simulationTime;
    // Convert to Mbps
    return throughputBps / 1000000.0;
}

// Helper function to configure WifiPhy attributes
void ConfigureWifiPhy(Ptr<YansWifiPhy> phy, FrequencyBand band, ChannelWidth width, bool sgi, bool preambleDetection)
{
    // Set frequency band
    uint32_t freq;
    if (band == BAND_2_4_GHZ)
    {
        freq = 2412; // Channel 1, 2.4 GHz
    }
    else // 5.0 GHz
    {
        freq = 5180; // Channel 36, 5 GHz
    }
    phy->Set("Frequency", UintegerValue(freq));

    // Set channel width
    phy->Set("ChannelWidth", UintegerValue(width == BW_20_MHZ ? 20 : 40));

    // Set short guard interval support
    phy->Set("ShortGuardIntervalSupported", BooleanValue(sgi));

    // Set preamble detection attribute for YansWifiPhy
    phy->Set("EnablePreambleDetection", BooleanValue(preambleDetection));
}

// Main simulation function for a single (streamCount, mcsIndex, distance) combination
double RunSimulation(int streamCount, int mcsIndex, double distance,
                     TransportProtocol protocol, FrequencyBand band, ChannelWidth width,
                     bool sgi, bool preambleDetection, double simTime, uint32_t packetSize)
{
    NS_LOG_INFO("Running simulation for " << streamCount << "x" << "MIMO, MCS " << mcsIndex
                                         << ", distance " << distance << "m, time " << simTime << "s");

    NodeContainer nodes;
    nodes.Create(2); // AP and STA

    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // 1. Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);

    apNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(distance, 0.0, 0.0));

    // 2. Wi-Fi setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); // Set to HT standard

    YansWifiChannelHelper channel;
    // Using LogDistancePropagationLossModel with exponent 3.0
    channel.SetPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(3.0));
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    // Configure PHY specific attributes
    ConfigureWifiPhy(phy, band, width, sgi, preambleDetection);
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Set remote station manager to ConstantRateWifiManager to fix MCS
    std::string dataMode = "HtMcs" + std::to_string(streamCount) + "x" + std::to_string(mcsIndex);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(dataMode),
                                 "ControlMode", StringValue("HtMcs0")); // Use a robust control mode

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ht-mimo-ssid");

    // AP MAC configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(1024)));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // STA MAC configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false)); // STA doesn't need to probe
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    // 3. IP Stack setup
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 4. Applications setup
    uint16_t port = 9;
    Ptr<PacketSink> sinkApp;

    if (protocol == UDP)
    {
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(apNode);
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simTime + 1.0)); // Stop server slightly after client

        UdpClientHelper client(apInterfaces.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000000)); // Large enough number
        client.SetAttribute("Interval", TimeValue(NanoSeconds(100))); // Max rate
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = client.Install(staNode);
        clientApps.Start(Seconds(1.0)); // Start client after 1 second
        clientApps.Stop(Seconds(simTime));

        sinkApp = DynamicCast<PacketSink>(serverApps.Get(0));
    }
    else // TCP
    {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(apNode);
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simTime + 1.0)); // Stop sink slightly after client

        BulkSendHelper client("ns3::TcpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), port));
        client.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely
        client.SetAttribute("SendSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = client.Install(staNode);
        clientApps.Start(Seconds(1.0)); // Start client after 1 second
        clientApps.Stop(Seconds(simTime));

        sinkApp = DynamicCast<PacketSink>(sinkApps.Get(0));
    }

    // 5. Simulation run
    Simulator::Stop(Seconds(simTime + 1.1)); // Ensure all traffic is processed
    Simulator::Run();

    double throughput = CalcThroughput(sinkApp, simTime);

    Simulator::Destroy(); // Clean up simulation resources

    return throughput;
}

int main(int argc, char *argv[])
{
    // LogComponentEnable("HtMimoSimulation", LOG_LEVEL_INFO);
    // LogComponentEnable("YansWifiPhy", LOG_LEVEL_DEBUG);
    // LogComponentEnable("ConstantRateWifiManager", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol", "Transport protocol (0 for UDP, 1 for TCP)", (uint32_t&)g_transportProtocol);
    cmd.AddValue("frequency", "Frequency band (0 for 2.4 GHz, 1 for 5.0 GHz)", (uint32_t&)g_frequencyBand);
    cmd.AddValue("channelWidth", "Channel width (0 for 20 MHz, 1 for 40 MHz)", (uint32_t&)g_channelWidth);
    cmd.AddValue("sgi", "Enable Short Guard Interval (true/false)", g_shortGuardInterval);
    cmd.AddValue("preambleDetection", "Enable Preamble Detection (true/false)", g_preambleDetection);
    cmd.AddValue("minDistance", "Minimum distance between AP and STA (m)", g_minDistance);
    cmd.AddValue("maxDistance", "Maximum distance between AP and STA (m)", g_maxDistance);
    cmd.AddValue("distanceStep", "Distance step size (m)", g_distanceStep);
    cmd.AddValue("simTime", "Simulation time per run (s)", g_simulationTime);
    cmd.AddValue("packetSize", "Application packet size (bytes)", g_packetSize);
    cmd.AddValue("maxMimoStreams", "Maximum MIMO streams to simulate (1-4)", g_maxMimoStreams);
    cmd.AddValue("mcsIndex", "Specific MCS index to simulate (0-7). -1 for all.", g_specificMcsIndex);
    cmd.AddValue("plotFilePrefix", "Prefix for Gnuplot output files", g_plotFileNamePrefix);

    cmd.Parse(argc, argv);

    // Validate parameters
    if (g_minDistance <= 0 || g_maxDistance < g_minDistance || g_distanceStep <= 0)
    {
        NS_FATAL_ERROR("Invalid distance parameters. minDistance, maxDistance, and distanceStep must be positive, and maxDistance >= minDistance.");
    }
    if (g_simulationTime <= 0)
    {
        NS_FATAL_ERROR("Simulation time must be positive.");
    }
    if (g_packetSize == 0)
    {
        NS_FATAL_ERROR("Packet size must be positive.");
    }
    if (g_maxMimoStreams < 1 || g_maxMimoStreams > 4)
    {
        NS_FATAL_ERROR("Max MIMO streams must be between 1 and 4.");
    }
    if (g_specificMcsIndex != -1 && (g_specificMcsIndex < 0 || g_specificMcsIndex > 7))
    {
        NS_FATAL_ERROR("Specific MCS index must be between 0 and 7, or -1 for all.");
    }

    // Map to store results: streamCount -> MCS Index -> vector of (distance, throughput) pairs
    std::map<int, std::map<int, std::vector<std::pair<double, double>>>> m_results;

    for (int streamCount = 1; streamCount <= g_maxMimoStreams; ++streamCount)
    {
        for (int mcsIndex = 0; mcsIndex <= 7; ++mcsIndex) // Standard MCS indices 0-7 for each Nss
        {
            if (g_specificMcsIndex != -1 && mcsIndex != g_specificMcsIndex)
            {
                continue; // Skip if specific MCS index is requested and doesn't match
            }

            NS_LOG_INFO("Starting simulation for " << streamCount << "x" << "MIMO, MCS " << mcsIndex);

            std::vector<std::pair<double, double>> current_mcs_plot_data;
            for (double distance = g_minDistance; distance <= g_maxDistance; distance += g_distanceStep)
            {
                double throughput = RunSimulation(streamCount, mcsIndex, distance,
                                                  g_transportProtocol, g_frequencyBand, g_channelWidth,
                                                  g_shortGuardInterval, g_preambleDetection,
                                                  g_simulationTime, g_packetSize);
                current_mcs_plot_data.push_back(std::make_pair(distance, throughput));
            }
            m_results[streamCount][mcsIndex] = current_mcs_plot_data;
        }
    }

    // Generate Gnuplot scripts and data files
    for (int streamCount = 1; streamCount <= g_maxMimoStreams; ++streamCount)
    {
        if (m_results.count(streamCount) == 0)
        {
            continue; // No data for this stream count
        }

        std::string plotFileName = g_plotFileNamePrefix + "_MIMO_" + std::to_string(streamCount) + "x" + ".plt";
        Gnuplot gnuplot(plotFileName);
        gnuplot.SetTitle("Throughput vs. Distance for " + std::to_string(streamCount) + " MIMO Stream(s)");
        gnuplot.SetXLabel("Distance (m)");
        gnuplot.SetYLabel("Throughput (Mbps)");
        gnuplot.AppendExtra("set yrange [0:]");
        gnuplot.AppendExtra("set grid");

        for (int mcsIndex = 0; mcsIndex <= 7; ++mcsIndex)
        {
            if (g_specificMcsIndex != -1 && mcsIndex != g_specificMcsIndex)
            {
                continue; // Skip if specific MCS index was requested and doesn't match
            }
            if (m_results[streamCount].count(mcsIndex) == 0)
            {
                continue; // No data for this MCS
            }

            GnuplotDataset dataset;
            dataset.SetTitle("MCS " + std::to_string(mcsIndex));
            dataset.SetStyle("linespoints");
            for (const auto& pair : m_results[streamCount][mcsIndex])
            {
                dataset.Add(pair.first, pair.second);
            }
            gnuplot.AddDataset(dataset);
        }
        std::ofstream plotFile(plotFileName.c_str());
        gnuplot.GenerateOutput(plotFile);
        plotFile.close();

        NS_LOG_INFO("Generated Gnuplot script: " << plotFileName);
    }

    return 0;
}