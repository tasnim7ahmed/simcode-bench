#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <limits> // For numeric_limits

// Required to use ns3 namespace and avoid typing ns3:: everywhere
using namespace ns3;

// Global variables for simulation configuration, accessible via command-line
double simTime = 10.0; // seconds
double distance = 1.0; // meters
std::string phyModel = "YansWifiPhy"; // Options: "YansWifiPhy", "SpectrumWifiPhy"
bool enablePcap = false;
bool useTcp = false; // true for TCP, false for UDP
uint32_t packetSize = 1024; // bytes
std::string dataRate = "50Mbps"; // For UDP (ignored for TCP which is rate adaptive)
std::string outputFile = "wifi-n-simulation-results.txt";

// Global variables for collecting trace data from PhyRxTrace
struct WifiRxInfo {
    double rssi;  // Received Signal Strength Indicator in dBm
    double noise; // Noise power in dBm
    double snr;   // Signal to Noise Ratio in dB
    double time;  // Time of reception
};
std::vector<WifiRxInfo> g_rxInfos;

// Callback function for PhyRxTrace to capture signal/noise information
void
PhyRxTrace (Ptr<const Packet> packet, const WifiMacHeader &hdr, const WifiPhy::RxPayloadState &state)
{
    // Collect Rx information for each received packet
    WifiRxInfo info;
    info.rssi = state.m_rxPowerDb;
    info.noise = state.m_noisePowerDb;
    info.snr = state.m_rxPowerDb - state.m_noisePowerDb;
    info.time = Simulator::Now().GetSeconds();
    g_rxInfos.push_back(info);
}

// Struct to store and report final simulation results for each configuration
struct SimulationResult {
    int mcs;
    int channelWidth;       // Channel width in MHz (20 or 40)
    double guardIntervalNs; // Guard interval in nanoseconds (400 or 800)
    double throughputMbps;
    double avgRssiDb;
    double avgNoiseDb;
    double avgSnrDb;
};
std::vector<SimulationResult> g_results;

int
main (int argc, char *argv[])
{
    // Command-line argument parsing
    CommandLine cmd (__FILE__);
    cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue ("distance", "Distance between nodes in meters", distance);
    cmd.AddValue ("phyModel", "Wi-Fi PHY model (YansWifiPhy or SpectrumWifiPhy)", phyModel);
    cmd.AddValue ("enablePcap", "Enable packet capture (true/false)", enablePcap);
    cmd.AddValue ("useTcp", "Use TCP traffic (true) or UDP traffic (false)", useTcp);
    cmd.AddValue ("packetSize", "Application packet size in bytes", packetSize);
    cmd.AddValue ("dataRate", "Application data rate for UDP (e.g., 50Mbps)", dataRate);
    cmd.AddValue ("outputFile", "Output file for simulation results", outputFile);
    cmd.Parse (argc, argv);

    // Iteration values for MCS, Channel Width, and Guard Interval
    std::vector<int> mcsValues = {0, 1, 2, 3, 4, 5, 6, 7}; // 802.11n HT MCS indices
    std::vector<int> channelWidthValues = {20, 40};        // Channel widths in MHz
    std::vector<double> guardIntervalValues = {800.0, 400.0}; // Guard intervals in ns (Long GI: 800ns, Short GI: 400ns)

    // Open output file for results
    std::ofstream ofs (outputFile);
    ofs << "MCS,ChannelWidth_MHz,GuardInterval_ns,Throughput_Mbps,AvgRSSI_dBm,AvgNoise_dBm,AvgSNR_dB\n";

    // Loop through all specified configurations
    for (int currentMcs : mcsValues) {
        for (int currentChannelWidth : channelWidthValues) {
            for (double currentGuardInterval : guardIntervalValues) {
                // Clear trace data from previous runs
                g_rxInfos.clear();

                // 1. Node Creation
                NodeContainer nodes;
                nodes.Create (2); // Two nodes: sender (0) and receiver (1)

                // 2. Mobility Model (Constant Position)
                MobilityHelper mobility;
                Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
                positionAlloc->Add (Vector (0.0, 0.0, 0.0));
                positionAlloc->Add (Vector (distance, 0.0, 0.0)); // Receiver at 'distance' from sender
                mobility.SetPositionAllocator (positionAlloc);
                mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
                mobility.Install (nodes);

                // 3. Wi-Fi Configuration
                WifiMacHelper wifiMac;
                wifiMac.SetType ("ns3::AdhocWifiMac"); // Ad-hoc mode for direct communication without an AP

                WifiHelper wifi;
                wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ); // Set to 802.11n standard (5GHz band)

                // Configure ConstantRateWifiManager to use specific MCS
                std::string mcsString = "HtMcs" + std::to_string (currentMcs);
                wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                              "DataMode", StringValue (mcsString),
                                              "ControlMode", StringValue ("HtMcs0")); // Use MCS0 for control frames

                Ptr<WifiPhy> receiverPhy; // Declare pointer to receiver's PHY

                NetDeviceContainer devices;
                if (phyModel == "YansWifiPhy") {
                    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
                    YansWifiPhyHelper phy;
                    phy.SetChannel (channel.Create ());
                    // Configure channel width and guard interval on the PHY layer
                    phy.Set ("ChannelWidth", UintegerValue (currentChannelWidth));
                    phy.Set ("GuardInterval", TimeValue (NanoSeconds (currentGuardInterval)));
                    devices = wifi.Install (phy, wifiMac, nodes);
                } else if (phyModel == "SpectrumWifiPhy") {
                    SpectrumWifiChannelHelper channel;
                    // Add propagation loss model for SpectrumWifiPhy (e.g., LogDistance)
                    channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                                "Exponent", DoubleValue (3.0)); // Default exponent
                    // Set propagation delay model
                    channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

                    SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
                    phy.SetChannel (channel.Create ());
                    // Configure channel width and guard interval on the PHY layer
                    phy.Set ("ChannelWidth", UintegerValue (currentChannelWidth));
                    phy.Set ("GuardInterval", TimeValue (NanoSeconds (currentGuardInterval)));
                    devices = wifi.Install (phy, wifiMac, nodes);
                } else {
                    NS_FATAL_ERROR ("Unknown PHY model: " << phyModel << ". Use 'YansWifiPhy' or 'SpectrumWifiPhy'.");
                }

                // Get the WifiPhy object of the receiver node for tracing
                receiverPhy = DynamicCast<WifiNetDevice>(devices.Get(1))->GetPhy();

                // 4. Internet Stack Installation
                InternetStackHelper stack;
                stack.Install (nodes);

                // 5. IP Address Assignment
                Ipv4AddressHelper address;
                address.SetBase ("10.1.1.0", "255.255.255.0");
                Ipv4InterfaceContainer interfaces = address.Assign (devices);

                // 6. Application Layer (Traffic Generation)
                uint16_t port = 9; // Common port for applications
                ApplicationContainer serverApps;
                ApplicationContainer clientApps;

                if (useTcp) {
                    // TCP traffic (OnOff application with PacketSink)
                    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                                           InetSocketAddress (Ipv4Address::Any (), port));
                    serverApps = sink.Install (nodes.Get (1)); // Sink on receiver
                    serverApps.Start (Seconds (0.0));
                    serverApps.Stop (Seconds (simTime));

                    OnOffHelper onoff ("ns3::TcpSocketFactory",
                                       InetSocketAddress (interfaces.GetAddress (1), port));
                    onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]")); // Always On
                    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]")); // No Off time
                    onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
                    onoff.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate))); // Target data rate for TCP
                    clientApps = onoff.Install (nodes.Get (0)); // OnOff client on sender
                    clientApps.Start (Seconds (1.0)); // Start client after server
                    clientApps.Stop (Seconds (simTime));
                } else {
                    // UDP traffic (UdpEchoClient/Server)
                    UdpEchoServerHelper echoServer (port);
                    serverApps = echoServer.Install (nodes.Get (1)); // Echo server on receiver
                    serverApps.Start (Seconds (0.0));
                    serverApps.Stop (Seconds (simTime));

                    UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
                    // Calculate interval to achieve the desired data rate with given packet size
                    double intervalSec = (double)packetSize * 8.0 / DataRate (dataRate).GetBitsPerSecond();
                    echoClient.SetAttribute ("MaxPackets", UintegerValue (std::numeric_limits<uint32_t>::max())); // Send many packets
                    echoClient.SetAttribute ("Interval", TimeValue (Seconds (intervalSec)));
                    echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
                    clientApps = echoClient.Install (nodes.Get (0)); // Echo client on sender
                    clientApps.Start (Seconds (1.0)); // Start client after server
                    clientApps.Stop (Seconds (simTime));
                }

                // 7. Tracing (Packet Capture)
                if (enablePcap) {
                    std::string pcapFileName = "wifi-n-sim-" + std::to_string(currentMcs) + "-"
                                                + std::to_string(currentChannelWidth) + "-"
                                                + std::to_string(static_cast<int>(currentGuardInterval)) + "-";
                    // Enable pcap for all devices
                    for (uint32_t i = 0; i < devices.GetN(); ++i) {
                        Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(devices.Get(i));
                        wifiDevice->GetPhy()->EnablePcap (pcapFileName, wifiDevice->GetNode(), i);
                    }
                }

                // Connect the PhyRxTrace callback to the receiver's PHY
                receiverPhy->TraceConnectWithoutContext ("PhyRxTrace", MakeCallback (&PhyRxTrace));

                // 8. Flow Monitor for throughput measurement
                FlowMonitorHelper flowmon;
                Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

                // 9. Run Simulation
                Simulator::Stop (Seconds (simTime));
                Simulator::Run ();

                // 10. Collect Results after simulation
                monitor->CheckForResults (); // Gather flow statistics

                Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
                FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

                double totalRxBytes = 0;
                // Iterate through flow stats to find the traffic from sender to receiver
                for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
                    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
                    if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1)) {
                        totalRxBytes += i->second.rxBytes;
                    }
                }
                // Calculate throughput in Mbps
                double throughputMbps = (totalRxBytes * 8.0) / (simTime * 1e6); // Bytes * 8 bits/Byte / (seconds * 10^6 bits/Mb)

                // Calculate average RSSI, Noise, and SNR from collected trace data
                double avgRssi = 0;
                double avgNoise = 0;
                double avgSnr = 0;
                if (!g_rxInfos.empty()) {
                    for (const auto& info : g_rxInfos) {
                        avgRssi += info.rssi;
                        avgNoise += info.noise;
                        avgSnr += info.snr;
                    }
                    avgRssi /= g_rxInfos.size();
                    avgNoise /= g_rxInfos.size();
                    avgSnr /= g_rxInfos.size();
                } else {
                    // Log a warning if no Rx trace data was collected (e.g., if packets were lost)
                    NS_LOG_WARN ("No PhyRxTrace data collected for MCS=" << currentMcs
                                                               << ", CW=" << currentChannelWidth
                                                               << ", GI=" << currentGuardInterval);
                }

                // Store current configuration's results
                SimulationResult result;
                result.mcs = currentMcs;
                result.channelWidth = currentChannelWidth;
                result.guardIntervalNs = currentGuardInterval;
                result.throughputMbps = throughputMbps;
                result.avgRssiDb = avgRssi;
                result.avgNoiseDb = avgNoise;
                result.avgSnrDb = avgSnr;
                g_results.push_back(result);

                // Write results to the output file immediately
                ofs << result.mcs << ","
                    << result.channelWidth << ","
                    << result.guardIntervalNs << ","
                    << result.throughputMbps << ","
                    << result.avgRssiDb << ","
                    << result.avgNoiseDb << ","
                    << result.avgSnrDb << "\n";
                ofs.flush(); // Flush the stream to ensure data is written

                // 11. Cleanup for the next simulation run
                Simulator::Destroy ();
            }
        }
    }

    ofs.close(); // Close the results file
    NS_LOG_UNCOND ("Simulation finished. Results saved to " << outputFile);

    return 0;
}