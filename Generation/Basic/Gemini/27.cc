#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

#include <iomanip> // For std::fixed and std::setprecision

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nGoodput");

int main (int argc, char *argv[])
{
    // Default values for command line arguments
    double simTime = 10.0; // Total simulation time in seconds
    bool isTcp = false; // Use TCP if true, UDP if false
    bool enableRtsCts = false; // Enable RTS/CTS if true
    double distance = 10.0; // Distance between AP and STA in meters
    double targetDataRateMbps = 100.0; // Target application data rate in Mbps (for UDP OnOffApp)
    int mcs = 7; // Modulation and Coding Scheme (0-7)
    uint32_t channelWidth = 40; // Channel width (20 or 40 MHz)
    std::string guardIntervalStr = "short"; // Guard interval ("short" or "long")

    // Configure command line arguments
    CommandLine cmd (__FILE__);
    cmd.AddValue ("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue ("isTcp", "Use TCP if true, UDP if false", isTcp);
    cmd.AddValue ("enableRtsCts", "Enable RTS/CTS if true", enableRtsCts);
    cmd.AddValue ("distance", "Distance between AP and STA in meters", distance);
    cmd.AddValue ("targetDataRateMbps", "Target application data rate in Mbps (for UDP OnOffApp)", targetDataRateMbps);
    cmd.AddValue ("mcs", "Modulation and Coding Scheme (0-7)", mcs);
    cmd.AddValue ("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue ("guardInterval", "Guard interval ('short' or 'long')", guardIntervalStr);
    cmd.Parse (argc, argv);

    // Validate MCS value
    if (mcs < 0 || mcs > 7)
    {
        NS_FATAL_ERROR ("Invalid MCS value. Must be between 0 and 7.");
    }
    // Validate channel width
    if (channelWidth != 20 && channelWidth != 40)
    {
        NS_FATAL_ERROR ("Invalid Channel Width. Must be 20 or 40 MHz.");
    }
    // Validate guard interval
    if (guardIntervalStr != "short" && guardIntervalStr != "long")
    {
        NS_FATAL_ERROR ("Invalid Guard Interval. Must be 'short' or 'long'.");
    }

    // Set up logging
    LogComponentEnable ("Wifi80211nGoodput", LOG_LEVEL_INFO);
    // LogComponentEnable ("HtWifiMac", LOG_LEVEL_DEBUG);
    // LogComponentEnable ("HtWifiManager", LOG_LEVEL_DEBUG);
    // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (2); // AP and STA

    Ptr<Node> apNode = nodes.Get (0);
    Ptr<Node> staNode = nodes.Get (1);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (distance),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (2),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Configure Wifi PHY and Channel
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::FriisPropagationLossModel"); // Simple path loss model

    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());
    // Set 802.11n specific PHY attributes
    phy.Set ("ChannelWidth", UintegerValue (channelWidth));
    phy.Set ("GuardInterval", StringValue (guardIntervalStr == "long" ? "Long" : "Short"));
    phy.Set ("PreambleMethod", StringValue ("Ht")); // For 802.11n High Throughput

    // Configure Wifi MAC
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ); // Use 802.11n 5GHz standard

    // Configure HtWifiManager to force the desired MCS
    // First, create an MCS set with only the desired MCS enabled
    McsSet mcsSet;
    mcsSet.Clear (); // Clear all default MCS values
    mcsSet.SetMcs (mcs); // Enable only the specified MCS

    // Set the HtWifiManager with the forced MCS set
    wifi.SetRemoteStationManager ("ns3::HtWifiManager",
                                  "SupportedMcsSet", McsSetValue (mcsSet));

    Ssid ssid = Ssid ("ns3-80211n-network");

    HtWifiMacHelper htMac;
    // Configure AP MAC
    htMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconInterval", TimeValue (MicroSeconds (102400)));
    // Enable/disable RTS/CTS threshold (0 forces RTS/CTS for all packets, large value disables)
    htMac.Set ("RtsCtsThreshold", UintegerValue (enableRtsCts ? 0 : 999999));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, htMac, apNode);

    // Configure STA MAC
    htMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
    // Enable/disable RTS/CTS threshold
    htMac.Set ("RtsCtsThreshold", UintegerValue (enableRtsCts ? 0 : 999999));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, htMac, staNode);

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // Install applications
    uint16_t port = 9; // Discard port
    ApplicationContainer clientApps, serverApps;

    if (isTcp)
    {
        // TCP Traffic (BulkSend from STA to AP)
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), port));
        serverApps = sinkHelper.Install (apNode); // AP is receiver
        serverApps.Start (Seconds (0.0));
        serverApps.Stop (Seconds (simTime));

        BulkSendHelper clientHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (apInterfaces.GetAddress (0), port));
        // MaxBytes = 0 means send forever (until application stops)
        clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
        clientApps = clientHelper.Install (staNode); // STA is sender
        clientApps.Start (Seconds (1.0)); // Start client after 1 second to allow connection setup
        clientApps.Stop (Seconds (simTime - 0.1)); // Stop client slightly before simulation ends
    }
    else
    {
        // UDP Traffic (OnOffApplication from STA to AP)
        PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), port));
        serverApps = sinkHelper.Install (apNode); // AP is receiver
        serverApps.Start (Seconds (0.0));
        serverApps.Stop (Seconds (simTime));

        OnOffHelper clientHelper ("ns3::UdpSocketFactory",
                                  InetSocketAddress (apInterfaces.GetAddress (0), port));
        clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate (targetDataRateMbps * 1e6))); // Convert Mbps to bps
        clientHelper.SetAttribute ("PacketSize", UintegerValue (1472)); // Typical MTU minus UDP/IP headers (1500 - 28)
        clientApps = clientHelper.Install (staNode); // STA is sender
        clientApps.Start (Seconds (1.0)); // Start client after 1 second
        clientApps.Stop (Seconds (simTime - 0.1)); // Stop client slightly before simulation ends
    }

    // Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Calculate and print goodput
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApps.Get (0));
    double goodputMbps = (sink->GetTotalRxBytes () * 8.0) / (simTime * 1e6); // Convert bytes to Mbps

    std::cout << "\n--- Simulation Results ---" << std::endl;
    std::cout << "  Scenario: 802.11n Wi-Fi (AP at (0,0,0), STA at (" << distance << ",0,0))" << std::endl;
    std::cout << "  MCS: " << mcs << std::endl;
    std::cout << "  Channel Width: " << channelWidth << " MHz" << std::endl;
    std::cout << "  Guard Interval: " << guardIntervalStr << std::endl;
    std::cout << "  Protocol: " << (isTcp ? "TCP" : "UDP") << std::endl;
    std::cout << "  RTS/CTS: " << (enableRtsCts ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Distance: " << distance << " m" << std::endl;
    if (!isTcp)
    {
        std::cout << "  Target Data Rate (UDP): " << targetDataRateMbps << " Mbps" << std::endl;
    }
    std::cout << "  Simulation Time: " << simTime << " s" << std::endl;
    std::cout << "  Total Bytes Received: " << sink->GetTotalRxBytes () << " bytes" << std::endl;
    std::cout << "  Goodput: " << std::fixed << std::setprecision (3) << goodputMbps << " Mbps" << std::endl;
    std::cout << "--------------------------\n" << std::endl;

    Simulator::Destroy ();
    return 0;
}