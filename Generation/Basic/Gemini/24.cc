#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot-helper.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath> // For std::log10

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiErrorModelComparison");

// Global variables for command-line parameters
static uint32_t g_packetSize = 1000; // bytes
static double g_startSnr = -5.0; // dB
static double g_endSnr = 25.0;   // dB
static double g_snrStep = 1.0;   // dB
static double g_txPowerDbm = 10.0; // dBm
static double g_distance = 1.0;  // meters
static double g_simulationTime = 1.0; // seconds per SNR point
static uint32_t g_numPacketsToSend = 10000; // target number of packets to send

int main (int argc, char *argv[])
{
    // Command-line parameter parsing
    CommandLine cmd (__FILE__);
    cmd.AddValue ("packetSize", "Size of application packets in bytes", g_packetSize);
    cmd.AddValue ("startSnr", "Starting SNR in dB", g_startSnr);
    cmd.AddValue ("endSnr", "Ending SNR in dB", g_endSnr);
    cmd.AddValue ("snrStep", "SNR step increment in dB", g_snrStep);
    cmd.AddValue ("txPower", "Transmit power in dBm", g_txPowerDbm);
    cmd.AddValue ("distance", "Distance between STA and AP in meters", g_distance);
    cmd.AddValue ("simulationTime", "Simulation time per SNR point in seconds", g_simulationTime);
    cmd.AddValue ("numPacketsToSend", "Number of packets to send per simulation run", g_numPacketsToSend);
    cmd.Parse (argc, argv);

    // Validate parameters
    if (g_simulationTime <= 0 || g_numPacketsToSend == 0 || g_snrStep <= 0)
    {
        NS_LOG_ERROR("Simulation time, number of packets to send, and SNR step must be positive.");
        return 1;
    }

    // List of MCS values to test
    std::vector<std::string> mcsModes = {
        "OfdmHtMcs0", // 6.5 Mbps for 20MHz, GI=800ns
        "OfdmHtMcs4", // 26 Mbps for 20MHz, GI=800ns
        "OfdmHtMcs7"  // 58.5 Mbps for 20MHz, GI=800ns
    };

    // Prepare Gnuplot datasets (one set of datasets per MCS)
    std::map<std::string, Gnuplot> gnuplots;
    std::map<std::string, GnuplotDataset> nistDatasets;
    std::map<std::string, GnuplotDataset> yansDatasets;
    std::map<std::string, GnuplotDataset> tableDatasets;

    for (const auto& mcsMode : mcsModes)
    {
        std::string currentPlotFileName = "fer-vs-snr-" + mcsMode + ".eps";
        gnuplots[mcsMode] = Gnuplot (currentPlotFileName, "Frame Error Rate vs. SNR (MCS: " + mcsMode + ")");
        gnuplots[mcsMode].SetTerminal ("postscript eps color enhanced");
        gnuplots[mcsMode].SetLegend ("SNR (dB)", "Frame Error Rate (FER)");
        gnuplots[mcsMode].AppendExtra ("set grid");
        gnuplots[mcsMode].AppendExtra ("set xrange [-5:30]"); // Adjust based on expected SNR range
        gnuplots[mcsMode].AppendExtra ("set yrange [0:1]");  // FER is between 0 and 1
        gnuplots[mcsMode].AppendExtra ("set key autotitle columnhead");

        nistDatasets[mcsMode] = GnuplotDataset ();
        nistDatasets[mcsMode].SetTitle ("Nist Error Model");
        nistDatasets[mcsMode].SetStyle ("linespoints");
        nistDatasets[mcsMode].SetExtra ("lc rgb 'red'");

        yansDatasets[mcsMode] = GnuplotDataset ();
        yansDatasets[mcsMode].SetTitle ("Yans Error Model");
        yansDatasets[mcsMode].SetStyle ("linespoints");
        yansDatasets[mcsMode].SetExtra ("lc rgb 'blue'");

        tableDatasets[mcsMode] = GnuplotDataset ();
        tableDatasets[mcsMode].SetTitle ("Table-based Error Model");
        tableDatasets[mcsMode].SetStyle ("linespoints");
        tableDatasets[mcsMode].SetExtra ("lc rgb 'green'");
    }

    // Nodes
    NodeContainer nodes;
    nodes.Create (2); // AP and STA

    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // Mobility (fixed position for simplicity)
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Set positions for AP and STA for path loss calculation
    Ptr<ConstantPositionMobilityModel> apMobility = apNode->GetObject<ConstantPositionMobilityModel> ();
    apMobility->SetPosition (Vector (0.0, 0.0, 0.0));
    Ptr<ConstantPositionMobilityModel> staMobility = staNode->GetObject<ConstantPositionMobilityModel> ();
    staMobility->SetPosition (Vector (g_distance, 0.0, 0.0));

    // Configure Wifi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n); // Use 802.11n for HT MCSs

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay ("ns3::ConstantPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());
    phy.Set ("TxPower", DoubleValue (g_txPowerDbm)); // Set fixed TxPower

    // YansWifiPhy has NoiseFigure attribute. Default is 10 dB.
    // It will be updated in the loop based on target SNR.
    phy.Set ("NoiseFigure", DoubleValue (10.0));

    WifiMacHelper mac;
    mac.SetType ("ns3::HtWifiMac",
                 "Ssid", SsidValue (Ssid ("ns3-wifi")),
                 "ActiveProbing", BooleanValue (false),
                 "QosSupported", BooleanValue (false)); // QosSupported needs to be false for some HT setups

    NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);
    NetDeviceContainer staDevices = wifi.Install (phy, mac, staNode);

    // Set up constant rate manager for specific MCS
    // This is applied to all devices, but will be overridden by Config::Set for DataMode.
    Config::Set ("/NodeList/*/DeviceList/*/Mac/RateManagerType", StringValue ("ns3::ConstantRateWifiManager"));
    // The specific DataMode (MCS) will be set in the loop via Config::Set path to HtWifiMac.

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);

    // Applications
    // Packet Sink (AP side)
    uint16_t port = 9;
    PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install (apNode);
    Ptr<PacketSink> packetSink = DynamicCast<PacketSink> (sinkApps.Get (0));
    sinkApps.Start (Seconds (0.0)); // App start time
    sinkApps.Stop (Seconds (g_simulationTime + 0.1)); // App stop time slightly after sim stop

    // OnOff Application (STA side)
    OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (apInterfaces.GetAddress (0), port));
    onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]")); // Always on
    onOffHelper.SetAttribute ("DataRate", StringValue ("100Mbps")); // Maximize data rate
    onOffHelper.SetAttribute ("PacketSize", UintegerValue (g_packetSize));
    onOffHelper.SetAttribute ("MaxPackets", UintegerValue (g_numPacketsToSend)); // Target packets
    ApplicationContainer onOffApps = onOffHelper.Install (staNode);
    Ptr<OnOffApplication> onOffApp = DynamicCast<OnOffApplication> (onOffApps.Get (0));
    onOffApps.Start (Seconds (0.0));
    onOffApps.Stop (Seconds (g_simulationTime + 0.1));

    // Get PHY pointers to set error models
    Ptr<WifiPhy> apPhy = DynamicCast<WifiNetDevice>(apDevices.Get(0))->GetPhy();
    Ptr<WifiPhy> staPhy = DynamicCast<WifiNetDevice>(staDevices.Get(0))->GetPhy();

    // Error models (created once)
    Ptr<NistErrorRateModel> nistErrorModel = CreateObject<NistErrorRateModel> ();
    Ptr<YansErrorRateModel> yansErrorModel = CreateObject<YansErrorRateModel> ();
    Ptr<TableBasedErrorRateModel> tableErrorModel = CreateObject<TableBasedErrorRateModel> ();

    // Calculate RxPower once (it's constant as TxPower and Distance are fixed)
    double frequencyGhz = 2.4; // Common assumption for 802.11n 2.4GHz
    // FriisPropagationLossModel in ns-3 uses 40.04 + 20*log10(f_GHz) + 20*log10(d_m)
    double pathLossDb = 40.04 + 20 * std::log10(frequencyGhz) + 20 * std::log10(g_distance);
    double rxPowerDbm = g_txPowerDbm - pathLossDb;
    NS_LOG_INFO("Calculated RxPower at " << g_distance << "m: " << rxPowerDbm << " dBm");


    // Loop through MCS values
    for (const auto& mcsMode : mcsModes)
    {
        NS_LOG_INFO ("--- Testing MCS: " << mcsMode << " ---");
        
        // Set the specific MCS mode for the MAC layer
        std::string dataModeString = "ns3::WifiMode::" + mcsMode;
        Config::Set ("/NodeList/*/DeviceList/*/Mac/HTManagement/DataMode", StringValue (dataModeString));
        // Also set for VHT/HE in case the MCS string matches, though for 802.11n, HT is primary.
        Config::Set ("/NodeList/*/DeviceList/*/Mac/VhtManagement/DataMode", StringValue (dataModeString));
        Config::Set ("/NodeList/*/DeviceList/*/Mac/HeManagement/DataMode", StringValue (dataModeString));

        // Loop through SNR values
        for (double snr = g_startSnr; snr <= g_endSnr; snr += g_snrStep)
        {
            NS_LOG_INFO ("  SNR: " << snr << " dB");

            // Calculate the required NoiseFigure for YansWifiPhy to achieve targetSnr
            // SNR = RxPower - NoiseFloor. Here, NoiseFloor is effectively controlled by NoiseFigure.
            // So, to achieve targetSnr, we set NoiseFigure = RxPower - targetSnr.
            double noiseFigureDb = rxPowerDbm - snr;
            Config::Set ("/NodeList/*/DeviceList/*/Phy/NoiseFigure", DoubleValue (noiseFigureDb));
            NS_LOG_DEBUG ("    Setting NoiseFigure: " << noiseFigureDb << " dB");

            // --- Run for Nist Model ---
            Simulator::Create (); // Clear previous state and create a new one
            packetSink->SetTotalRx (0); // Reset sink Rx counter
            onOffApp->SetMaxPackets (g_numPacketsToSend); // Re-arm MaxPackets for this run
            staPhy->SetErrorRateModel (nistErrorModel);
            apPhy->SetErrorRateModel (nistErrorModel);
            Simulator::Run ();
            uint64_t totalTx = g_numPacketsToSend; // Assume all packets were attempted to be sent within simTime
            uint64_t totalRx = packetSink->GetTotalRx ();
            double ferNist = (totalTx > 0) ? static_cast<double>(totalTx - totalRx) / totalTx : 1.0;
            nistDatasets[mcsMode].Add (snr, ferNist);
            NS_LOG_INFO ("    Nist FER: " << ferNist);
            Simulator::Destroy ();

            // --- Run for Yans Model ---
            Simulator::Create ();
            packetSink->SetTotalRx (0);
            onOffApp->SetMaxPackets (g_numPacketsToSend);
            staPhy->SetErrorRateModel (yansErrorModel);
            apPhy->SetErrorRateModel (yansErrorModel);
            Simulator::Run ();
            totalTx = g_numPacketsToSend;
            totalRx = packetSink->GetTotalRx ();
            double ferYans = (totalTx > 0) ? static_cast<double>(totalTx - totalRx) / totalTx : 1.0;
            yansDatasets[mcsMode].Add (snr, ferYans);
            NS_LOG_INFO ("    Yans FER: " << ferYans);
            Simulator::Destroy ();

            // --- Run for Table-based Model ---
            Simulator::Create ();
            packetSink->SetTotalRx (0);
            onOffApp->SetMaxPackets (g_numPacketsToSend);
            staPhy->SetErrorRateModel (tableErrorModel);
            apPhy->SetErrorRateModel (tableErrorModel);
            Simulator::Run ();
            totalTx = g_numPacketsToSend;
            totalRx = packetSink->GetTotalRx ();
            double ferTable = (totalTx > 0) ? static_cast<double>(totalTx - totalRx) / totalTx : 1.0;
            tableDatasets[mcsMode].Add (snr, ferTable);
            NS_LOG_INFO ("    Table-based FER: " << ferTable);
            Simulator::Destroy ();
        }
    }

    // Generate plots
    for (const auto& mcsMode : mcsModes)
    {
        gnuplots[mcsMode].AddDataset (nistDatasets[mcsMode]);
        gnuplots[mcsMode].AddDataset (yansDatasets[mcsMode]);
        gnuplots[mcsMode].AddDataset (tableDatasets[mcsMode]);
        gnuplots[mcsMode].GenerateOutput (std::cout);
    }
    
    return 0;
}