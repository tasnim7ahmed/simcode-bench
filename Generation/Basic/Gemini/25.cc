#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

// Global variables for simulation parameters, mostly configured via CommandLine
uint32_t g_numStaNodes = 1;
double g_distance = 10.0; // meters
bool g_useUdp = true;
uint32_t g_packetSize = 1024; // bytes
double g_dataRateMbps = 50.0; // Mbps, for UDP OnOffApplication
std::string g_wifiStandard = "80211ax"; // This will be set on WifiHelper
std::string g_phyModel = "Yans"; // Yans or Spectrum
std::string g_phyBand = "5GHz"; // 2.4GHz, 5GHz, 6GHz
uint32_t g_channelWidth = 80; // MHz: 20, 40, 80, 160, 8080 (for 80+80MHz)
uint32_t g_mcs = 7; // MCS index for 802.11ax (0-11)
uint32_t g_gi = 800; // Guard Interval in ns: 400 (short), 800 (normal), 1600 (long)
bool g_rtsCtsEnabled = false;
// bool g_extendedBa = false; // Extended Block Ack is typically default for HeWifiMac
bool g_ulOfdmaEnabled = false;
std::string g_dlMuPpduAckPolicy = "None"; // None, Multistation, SingleUser
double g_simTime = 10.0; // seconds

int main(int argc, char *argv[])
{
    // Configure command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("numStaNodes", "Number of station nodes", g_numStaNodes);
    cmd.AddValue("distance", "Distance between AP and stations (m)", g_distance);
    cmd.AddValue("useUdp", "Use UDP (true) or TCP (false) traffic", g_useUdp);
    cmd.AddValue("packetSize", "Packet payload size (bytes)", g_packetSize);
    cmd.AddValue("dataRateMbps", "Data rate for UDP OnOffApplication (Mbps)", g_dataRateMbps);
    cmd.AddValue("phyModel", "PHY Model (Yans or Spectrum)", g_phyModel);
    cmd.AddValue("phyBand", "PHY Band (2.4GHz, 5GHz, 6GHz)", g_phyBand);
    cmd.AddValue("channelWidth", "Channel Width (MHz): 20, 40, 80, 160, 8080 (for 80+80MHz)", g_channelWidth);
    cmd.AddValue("mcs", "MCS index (0-11 for 802.11ax)", g_mcs);
    cmd.AddValue("gi", "Guard Interval (ns): 400, 800, 1600", g_gi);
    cmd.AddValue("rtsCts", "Enable RTS/CTS", g_rtsCtsEnabled);
    cmd.AddValue("ulOfdma", "Enable UL OFDMA", g_ulOfdmaEnabled);
    cmd.AddValue("dlMuPpduAckPolicy", "DL MU PPDU Ack Policy (None, Multistation, SingleUser)", g_dlMuPpduAckPolicy);
    cmd.AddValue("simTime", "Simulation time (seconds)", g_simTime);
    cmd.Parse(argc, argv);

    // Validate inputs
    if (g_numStaNodes == 0)
    {
        std::cerr << "Error: Number of station nodes must be greater than 0" << std::endl;
        return 1;
    }
    if (g_channelWidth != 20 && g_channelWidth != 40 && g_channelWidth != 80 && g_channelWidth != 160 && g_channelWidth != 8080)
    {
        std::cerr << "Error: Invalid channel width. Supported: 20, 40, 80, 160, 8080 (for 80+80MHz)" << std::endl;
        return 1;
    }
    if (g_phyBand != "2.4GHz" && g_phyBand != "5GHz" && g_phyBand != "6GHz")
    {
        std::cerr << "Error: Invalid PHY band. Supported: 2.4GHz, 5GHz, 6GHz" << std::endl;
        return 1;
    }
    if (g_gi != 400 && g_gi != 800 && g_gi != 1600)
    {
        std::cerr << "Error: Invalid Guard Interval. Supported: 400, 800, 1600" << std::endl;
        return 1;
    }
    if (g_mcs > 11) // For 802.11ax, MCS 0-11
    {
        std::cerr << "Error: Invalid MCS for 802.11ax. Supported: 0-11" << std::endl;
        return 1;
    }
    if (g_dlMuPpduAckPolicy != "None" && g_dlMuPpduAckPolicy != "Multistation" && g_dlMuPpduAckPolicy != "SingleUser")
    {
        std::cerr << "Error: Invalid DL MU PPDU Ack Policy. Supported: None, Multistation, SingleUser" << std::endl;
        return 1;
    }

    // Set up nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(g_numStaNodes);

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(g_distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // Set up WiFi PHY and Channel
    Ptr<YansWifiChannel> yansChannel;
    Ptr<SpectrumWifiChannel> spectrumChannel;
    WifiPhyHelper phy;

    if (g_phyModel == "Yans")
    {
        yansChannel = CreateObject<YansWifiChannel>();
        yansChannel->SetPropagationLossModel(CreateObject<FriisPropagationLossModel>());
        yansChannel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
        phy.SetChannel(yansChannel);
    }
    else if (g_phyModel == "Spectrum")
    {
        spectrumChannel = CreateObject<SpectrumWifiChannel>();
        spectrumChannel->AddPropagationLoss(CreateObject<FriisPropagationLossModel>());
        spectrumChannel->AddPropagationDelay(CreateObject<ConstantSpeedPropagationDelayModel>());
        phy.SetChannel(spectrumChannel);
    }
    else
    {
        std::cerr << "Error: Invalid PHY model. Use Yans or Spectrum." << std::endl;
        return 1;
    }

    // Set standard to 802.11ax
    phy.SetStandard(WIFI_PHY_STANDARD_80211ax);

    // Configure frequency band and channel number
    uint32_t centralFreq = 0;
    uint32_t channelNumber = 0;
    if (g_phyBand == "2.4GHz")
    {
        centralFreq = 2412; // Example for 2.4GHz (Channel 1)
        channelNumber = 1;
    }
    else if (g_phyBand == "5GHz")
    {
        centralFreq = 5180; // Example for 5GHz (Channel 36)
        channelNumber = 36;
    }
    else if (g_phyBand == "6GHz")
    {
        centralFreq = 5955; // Example for 6GHz (Channel 1)
        channelNumber = 1;
    }
    phy.Set("Frequency", UintegerValue(centralFreq));
    phy.Set("ChannelNumber", UintegerValue(channelNumber));

    // Configure channel width
    WifiChannelWidth wifiChannelWidth;
    if (g_channelWidth == 20)
    {
        wifiChannelWidth = WIFI_CHANNEL_WIDTH_20MHZ;
    }
    else if (g_channelWidth == 40)
    {
        wifiChannelWidth = WIFI_CHANNEL_WIDTH_40MHZ;
    }
    else if (g_channelWidth == 80)
    {
        wifiChannelWidth = WIFI_CHANNEL_WIDTH_80MHZ;
    }
    else if (g_channelWidth == 160)
    {
        wifiChannelWidth = WIFI_CHANNEL_WIDTH_160MHZ;
    }
    else if (g_channelWidth == 8080)
    {
        wifiChannelWidth = WIFI_CHANNEL_WIDTH_80PLUS80MHZ;
    }
    phy.SetChannelWidth(wifiChannelWidth);

    // Configure Guard Interval
    phy.SetGuardInterval(static_cast<WifiGuardInterval>(g_gi));

    // Set up Wifi MAC
    WifiMacHelper mac;
    WifiHelper wifi;

    // Set standard on WifiHelper to ensure consistency
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ax);

    // Rate control: Use ConstantRateWifiManager to force a specific MCS
    std::string phyMode = "HeMcs" + std::to_string(g_mcs);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    // Apply global HeWifiMac attributes using Config::SetDefault
    Config::SetDefault("ns3::HeWifiMac::DlMuPpduAckPolicy", EnumValue(HeWifiMac::NO_ACK));
    if (g_dlMuPpduAckPolicy == "Multistation")
    {
        Config::SetDefault("ns3::HeWifiMac::DlMuPpduAckPolicy", EnumValue(HeWifiMac::MULTISTATION_ACK));
    }
    else if (g_dlMuPpduAckPolicy == "SingleUser")
    {
        Config::SetDefault("ns3::HeWifiMac::DlMuPpduAckPolicy", EnumValue(HeWifiMac::SINGLE_USER_ACK));
    }
    if (g_ulOfdmaEnabled)
    {
        Config::SetDefault("ns3::HeWifiMac::UplinkOfdmaSupported", BooleanValue(true));
    }
    // Extended Block Ack: HeWifiMac typically supports and enables this by default for QoS data frames.
    // No explicit attribute for user to toggle typically.

    // Configure RTS/CTS globally
    if (g_rtsCtsEnabled)
    {
        Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", UintegerValue(0)); // 0 means always on
    }
    else
    {
        Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", UintegerValue(65535)); // Max value means never on
    }

    // Install Wi-Fi devices for AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns3-wifi-ax")),
                "BeaconInterval", TimeValue(MicroSeconds(102400)),
                "HeOperation", BooleanValue(true)); // Enable HE Operation for 802.11ax
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // Install Wi-Fi devices for STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns3-wifi-ax")),
                "ActiveProbing", BooleanValue(false), // Disable active probing for simplicity
                "AssociationRequestTimeout", TimeValue(Seconds(1)),
                "HeOperation", BooleanValue(true)); // Enable HE Operation for 802.11ax
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Install IP Stack
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIpIfs = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer staIpIfs = ipv4.Assign(staDevices);

    // Applications
    uint16_t port = 9; // Discard port
    ApplicationContainer clientApps, serverApps;

    if (g_useUdp)
    {
        // UDP Server on AP
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApps.Add(sink.Install(apNode.Get(0)));

        // UDP Client on each STA (sending to AP)
        OnOffHelper client("ns3::UdpSocketFactory", Address());
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate(g_dataRateMbps * 1024 * 1024)));
        client.SetAttribute("PacketSize", UintegerValue(g_packetSize));

        for (uint32_t i = 0; i < g_numStaNodes; ++i)
        {
            AddressValue remoteAddress(InetSocketAddress(apIpIfs.GetAddress(0), port));
            client.SetAttribute("Remote", remoteAddress);
            clientApps.Add(client.Install(staNodes.Get(i)));
        }
    }
    else
    {
        // TCP Server on AP
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApps.Add(sink.Install(apNode.Get(0)));

        // TCP Client on each STA (sending to AP)
        BulkSendHelper client("ns3::TcpSocketFactory", Address());
        client.SetAttribute("Remote", AddressValue(InetSocketAddress(apIpIfs.GetAddress(0), port)));
        client.SetAttribute("MaxBytes", UintegerValue(0)); // Send until simulation end
        client.SetAttribute("SendSize", UintegerValue(g_packetSize));

        for (uint32_t i = 0; i < g_numStaNodes; ++i)
        {
            clientApps.Add(client.Install(staNodes.Get(i)));
        }
    }

    // Start and Stop Applications
    serverApps.Start(Seconds(0.0));
    clientApps.Start(Seconds(1.0)); // Start clients slightly after server
    serverApps.Stop(Seconds(g_simTime));
    clientApps.Stop(Seconds(g_simTime));

    // Flow Monitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Simulation run
    Simulator::Stop(Seconds(g_simTime + 1.0)); // Give some extra time for final events
    Simulator::Run();

    // Flow Monitor statistics
    monitor->CheckFor</td>
</tr>
<tr class="odd">
<td align="left">Simulator::Destroy();

    // Output results
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::Stats stats = monitor->GetStats();

    double totalThroughputMbps = 0.0;
    uint32_t totalRxBytes = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // We are interested in traffic from STAs to AP (uplink) as clients are STAs
        // and server is AP.
        if (t.destinationAddress == apIpIfs.GetAddress(0)) // Flow is destined for the AP (uplink)
        {
            totalRxBytes += i->second.rxBytes;
        }
    }

    totalThroughputMbps = (totalRxBytes * 8.0) / (g_simTime * 1000000.0);

    std::cout << "Simulation Results:" << std::endl;
    std::cout << "  Number of STA nodes: " << g_numStaNodes << std::endl;
    std::cout << "  Distance: " << g_distance << " m" << std::endl;
    std::cout << "  Traffic Type: " << (g_useUdp ? "UDP" : "TCP") << std::endl;
    std::cout << "  Packet Size: " << g_packetSize << " bytes" << std::endl;
    if (g_useUdp)
    {
        std::cout << "  UDP Data Rate: " << g_dataRateMbps << " Mbps" << std::endl;
    }
    std::cout << "  Wi-Fi Standard: " << g_wifiStandard << std::endl;
    std::cout << "  PHY Model: " << g_phyModel << std::endl;
    std::cout << "  PHY Band: " << g_phyBand << std::endl;
    std::cout << "  Channel Width: ";
    if (g_channelWidth == 8080) {
        std::cout << "80+80 MHz";
    } else {
        std::cout << g_channelWidth << " MHz";
    }
    std::cout << std::endl;
    std::cout << "  MCS: " << g_mcs << std::endl;
    std::cout << "  Guard Interval: " << g_gi << " ns" << std::endl;
    std::cout << "  RTS/CTS Enabled: " << (g_rtsCtsEnabled ? "Yes" : "No") << std::endl;
    std::cout << "  UL OFDMA Enabled: " << (g_ulOfdmaEnabled ? "Yes" : "No") << std::endl;
    std::cout << "  DL MU PPDU Ack Policy: " << g_dlMuPpduAckPolicy << std::endl;
    std::cout << "  Simulation Time: " << g_simTime << " s" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "  Aggregated Uplink Throughput: " << totalThroughputMbps << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}