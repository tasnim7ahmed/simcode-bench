#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/error-model-module.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cmath>

NS_LOG_COMPONENT_DEFINE("WifiSpectrumInterferer");

// Global variables to store simulation parameters with default values
double g_simTime = 10.0; // seconds
double g_distance = 50.0; // meters
uint32_t g_mcsIndex = 7; // MCS index for 802.11n (e.g., MCS 7: 65 Mbps in 20MHz, short GI)
uint32_t g_channelWidth = 20; // MHz (20 or 40)
uint32_t g_guardInterval = 0; // 0 for short GI, 1 for long GI
double g_interfererPowerDb = -30.0; // dBm, Non-Wi-Fi Interferer power
std::string g_wifiType = "802.11n"; // Wifi standard (e.g., 802.11n, 82.11ac)
std::string g_errorModelType = "ns3::NistErrorModel"; // Error model type
bool g_enablePcap = false; // Enable PCAP tracing
bool g_enableLogs = true; // Enable detailed logging of RSSI/SNR etc.

/**
 * \brief Trace callback function for SpectrumWifiPhy's "MonitorSnrTrace".
 * This function provides per-packet received signal strength (RSSI), noise, and SNR
 * for successfully detected frames, as seen by the PHY layer before decoding.
 * Note: This trace provides pre-detection values, not post-demodulation.
 * \param context The context string for the trace source (e.g., node name).
 * \param params Pointer to the SpectrumPhyRxParameters containing signal, noise PSDs.
 */
void
MonitorSnrTrace(std::string context, Ptr<const SpectrumPhyRxParameters> params)
{
    if (g_enableLogs)
    {
        // Extract required parameters
        // The PSDs are given in W/Hz. To get total power, integrate over the bandwidth.
        // Assuming a flat PSD over the Wi-Fi channel width.
        double channelWidthHz = g_channelWidth * 1e6; // Convert MHz to Hz

        double signalPowerWatts = params->signalPowerSpectralDensity->Integrate(channelWidthHz);
        double noisePowerWatts = params->noisePowerSpectralDensity->Integrate(channelWidthHz);

        // Convert powers to dBm (1 W = 1000 mW)
        double signalPowerDb = 10 * std::log10(signalPowerWatts * 1000.0);
        double noisePowerDb = 10 * std::log10(noisePowerWatts * 1000.0);

        // Calculate SNR
        double snr = signalPowerWatts / noisePowerWatts;
        double snrDb = 10 * std::log10(snr);

        NS_LOG_INFO("Time: " << Simulator::Now().GetSeconds() << "s | Node: " << context
                              << " | RSSI: " << signalPowerDb << " dBm"
                              << " | Noise: " << noisePowerDb << " dBm"
                              << " | SNR: " << snrDb << " dB");
    }
}


int
main(int argc, char* argv[])
{
    // 1. Command-line argument parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", g_simTime);
    cmd.AddValue("distance", "Distance between Wi-Fi nodes in meters", g_distance);
    cmd.AddValue("mcsIndex", "MCS index for 802.11n (0-7 for 20MHz, short GI)", g_mcsIndex);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", g_channelWidth);
    cmd.AddValue("guardInterval", "Guard Interval (0 for short GI, 1 for long GI)", g_guardInterval);
    cmd.AddValue("interfererPower", "Non-Wi-Fi Interferer Power in dBm", g_interfererPowerDb);
    cmd.AddValue("wifiType", "Wi-Fi Standard (e.g., 802.11n)", g_wifiType);
    cmd.AddValue("errorModelType", "Error model type (e.g., ns3::NistErrorModel, ns3::RateAdaptationErrorModel)", g_errorModelType);
    cmd.AddValue("enablePcap", "Enable PCAP tracing (true/false)", g_enablePcap);
    cmd.AddValue("enableLogs", "Enable detailed RSSI/SNR logs (true/false)", g_enableLogs);
    cmd.Parse(argc, argv);

    // Validate parameters
    if (g_channelWidth != 20 && g_channelWidth != 40)
    {
        NS_FATAL_ERROR("Channel width must be 20 or 40 MHz.");
    }
    // MCS index for 802.11n 1x1 Spatial Streams (HT rates) goes from 0 to 7.
    // For 20MHz, short GI, MCS 7 is 65 Mbps. For 40MHz, short GI, MCS 7 is 135 Mbps.
    if (g_mcsIndex > 7)
    {
        NS_LOG_WARN("MCS Index for 802.11n should typically be between 0 and 7 for 1x1 spatial streams. You selected: " << g_mcsIndex);
    }
    if (g_guardInterval != 0 && g_guardInterval != 1)
    {
        NS_FATAL_ERROR("Guard interval must be 0 (short) or 1 (long).");
    }

    // Configure logging for relevant modules
    if (g_enableLogs)
    {
        LogComponentEnable("WifiSpectrumInterferer", LOG_LEVEL_INFO);
        LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
        LogComponentEnable("SpectrumWifiPhy", LOG_LEVEL_INFO);
        LogComponentEnable("QosWifiMac", LOG_LEVEL_INFO); // For 802.11n/ac
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
        // LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO); // Too verbose for general purpose
    }

    // 2. Create Nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2); // Node 0: AP, Node 1: STA

    // 3. Configure Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Set positions for AP and STA
    Ptr<ConstantPositionMobilityModel> apMobility = wifiNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    Ptr<ConstantPositionMobilityModel> staMobility = wifiNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    staMobility->SetPosition(Vector(g_distance, 0.0, 0.0));

    // 4. Set up Wi-Fi
    // Wifi Channel Model: YansWifiChannelHelper creates a channel, SpectrumWifiPhyHelper uses it.
    // This allows for detailed interference modeling via SpectrumChannel.
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> yansChannel = channelHelper.Create();

    // Wifi PHY Model: SpectrumWifiPhy allows external spectrum sources (like our interferer)
    SpectrumWifiPhyHelper phyHelper = SpectrumWifiPhyHelper::Default();
    phyHelper.SetChannel(yansChannel);
    // Set the error model specified by command line
    phyHelper.SetErrorRateModel(g_errorModelType);
    phyHelper.Set("TxPowerStart", DoubleValue(18.0)); // Default Tx Power
    phyHelper.Set("TxPowerEnd", DoubleValue(18.0));

    // Wifi Standard and MAC
    WifiHelper wifiHelper;
    if (g_wifiType == "802.11n")
    {
        wifiHelper.SetStandard(WifiStandard::STANDARD_802_11n);
    }
    else if (g_wifiType == "802.11ac")
    {
        wifiHelper.SetStandard(WifiStandard::STANDARD_802_11ac);
    }
    else
    {
        NS_FATAL_ERROR("Unsupported Wi-Fi type: " << g_wifiType << ". Please use 802.11n or 802.11ac.");
    }

    // Configure WifiRemoteStationManager for fixed rate 802.11n (HT rates).
    // Using ConstantRateWifiManager to force a specific MCS, channel width, and guard interval.
    std::stringstream ss;
    ss << "HtMcs_MCS" << g_mcsIndex;
    if (g_channelWidth == 40)
    {
        ss << "_40MHz";
    }
    else // Default to 20MHz
    {
        ss << "_20MHz";
    }
    if (g_guardInterval == 0) // Short GI
    {
        ss << "_SGI";
    }
    else // Long GI
    {
        ss << "_LGI";
    }
    std::string phyMode = ss.str();

    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue(phyMode),
                                       "ControlMode", StringValue(phyMode));

    // Create MAC layer helpers for AP and STA
    QosWifiMacHelper wifiMac = QosWifiMacHelper::Default(); // QosWifiMac for 802.11n

    NetDeviceContainer apDevices, staDevices;

    // AP MAC configuration
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", Ssid("ns3-wifi"),
                    "BeaconInterval", TimeValue(MicroSeconds(102400))); // Default 802.11 beacon interval
    apDevices = wifiHelper.Install(phyHelper, wifiMac, wifiNodes.Get(0));

    // STA MAC configuration
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", Ssid("ns3-wifi"),
                    "ActiveProbing", BooleanValue(false));
    staDevices = wifiHelper.Install(phyHelper, wifiMac, wifiNodes.Get(1));

    // 5. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // 6. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 7. Non-Wi-Fi Interferer
    // Create a dummy node for the interferer and set its position (e.g., at AP's location)
    Ptr<Node> interfererNode = CreateObject<Node>();
    Ptr<ConstantPositionMobilityModel> interfererMobility = CreateObject<ConstantPositionMobilityModel>();
    interfererNode->AggregateObject(interfererMobility);
    interfererMobility->SetPosition(Vector(0.0, 0.0, 0.0)); // Co-located with AP for maximum impact

    // Create a SpectrumPhy that represents the interferer's transmission.
    // It's connected to the same SpectrumChannel as the Wi-Fi devices.
    Ptr<SpectrumPhy> interfererSpectrumPhy = CreateObject<SpectrumPhy>();
    interfererSpectrumPhy->SetChannel(phyHelper.GetChannel());
    interfererSpectrumPhy->SetDevice(interfererNode); // Associate with the interferer node

    // Define the interferer's power spectral density (PSD).
    // Let's assume the interferer operates across the same band as the Wi-Fi.
    double centerFrequencyMhz = 2437.0; // Wi-Fi Channel 6 center frequency for 2.4 GHz
    double startFrequencyMhz = centerFrequencyMhz - (g_channelWidth / 2.0);
    double endFrequencyMhz = centerFrequencyMhz + (g_channelWidth / 2.0);

    // Convert interferer power from dBm to Watts (total power)
    double interfererPowerWatts = std::pow(10.0, g_interfererPowerDb / 10.0) / 1000.0;
    // Distribute the total power over the specified bandwidth to get PSD (W/Hz)
    double interfererPsWattsPerHz = interfererPowerWatts / (g_channelWidth * 1e6);

    // Create a constant PSD source for the interferer
    Ptr<ConstantSpectrumPsSource> interfererPsSource = CreateObject<ConstantSpectrumPsSource>();
    interfererPsSource->SetConstantPower(interfererPsWattsPerHz);
    interfererPsSource->SetFrequencyRange(startFrequencyMhz * 1e6, endFrequencyMhz * 1e6); // Frequencies in Hz

    // Add the interferer source to the channel using InterferenceHelper
    InterferenceHelper interferenceHelper;
    interferenceHelper.CreateInterferenceSource(interfererSpectrumPhy, interfererPsSource);

    // 8. Install Applications (TCP and UDP)
    // UDP traffic: STA (client) to AP (server)
    uint16_t udpPort = 9; // Echo port
    UdpEchoServerHelper echoServer(udpPort);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0)); // AP is server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(g_simTime));

    UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), udpPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000)); // Send many packets
    echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(100))); // Small interval for high rate
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // 1KB packets
    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(1)); // STA is client
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(g_simTime));

    // TCP traffic: STA (client) to AP (server) - BulkSend
    uint16_t tcpPort = 50000;
    Address sinkAddress(InetSocketAddress(apInterfaces.GetAddress(0), tcpPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiNodes.Get(0)); // AP is sink
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(g_simTime));

    OnOffHelper onoffHelper("ns3::TcpSocketFactory", sinkAddress);
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always On
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never Off
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps"))); // Attempt to send at high rate
    onoffHelper.SetAttribute("PacketSize", UintegerValue(1472)); // Standard TCP payload size
    ApplicationContainer onoffApps = onoffHelper.Install(wifiNodes.Get(1)); // STA is source
    onoffApps.Start(Seconds(1.0));
    onoffApps.Stop(Seconds(g_simTime));

    // 9. Enable tracing
    if (g_enablePcap)
    {
        // Enable PCAP for AP and STA Wi-Fi devices
        phyHelper.EnablePcap("wifi-spectrum-interferer-ap", apDevices.Get(0), true);
        phyHelper.EnablePcap("wifi-spectrum-interferer-sta", staDevices.Get(0), true);
    }

    // Connect the custom trace sink to the SpectrumWifiPhy's "MonitorSnrTrace"
    // This will provide real-time SNR, RSSI, and Noise logs for the STA (receiver).
    Ptr<SpectrumWifiPhy> staPhy = DynamicCast<SpectrumWifiPhy>(staDevices.Get(0)->GetObject<WifiNetDevice>()->GetPhy());
    staPhy->TraceConnectWithoutContext("MonitorSnrTrace", MakeCallback(&MonitorSnrTrace, "STA"));

    // 10. Flow Monitor
    // Installs FlowMonitor on all nodes
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 11. Run Simulation
    Simulator::Stop(Seconds(g_simTime));
    Simulator::Run();

    // 12. Print Results
    // Collect and print FlowMonitor statistics
    monitor->CheckFor ; // Ensure all flows are captured
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->Get=FlowStats>();

    double totalThroughputMbps = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "\n--- Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") ---" << std::endl;
        std::cout << "  Protocol: " << (uint16_t)t.protocol << (t.protocol == 6 ? " (TCP)" : (t.protocol == 17 ? " (UDP)" : "")) << std::endl;
        std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
        std::cout << "  Tx Bytes:   " << i->second.txBytes << std::endl;
        std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << std::endl;
        std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;

        if (i->second.timeLastRxPacket.GetSeconds() > 0)
        {
            double flowDuration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
            if (flowDuration <= 0) flowDuration = g_simTime; // Avoid division by zero for very short flows

            double throughput = (double)i->second.rxBytes * 8 / flowDuration / 1000000.0; // Mbps
            std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
            totalThroughputMbps += throughput;

            if (i->second.rxPackets > 0)
            {
                std::cout << "  Total Delay:  " << i->second.delaySum.GetSeconds() << " s" << std::endl;
                std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s" << std::endl;
            }
        }
    }
    std::cout << "\n--- Simulation Summary ---" << std::endl;
    std::cout << "Total Network Throughput: " << totalThroughputMbps << " Mbps" << std::endl;
    std::cout << "Simulation End Time: " << g_simTime << " seconds" << std::endl;

    Simulator::Destroy();

    return 0;
}