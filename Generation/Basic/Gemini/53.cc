#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiNAggregation");

// Global vector to store max TXOP durations for each AP
std::vector<Time> g_maxTxopDurations;

// Callback function for TxopDuration trace source
void TxopDurationCallback(uint32_t apIndex, Time duration)
{
    // The vector g_maxTxopDurations is pre-sized in main
    if (duration > g_maxTxopDurations[apIndex])
    {
        g_maxTxopDurations[apIndex] = duration;
    }
}

int main(int argc, char *argv[])
{
    double distance = 10.0; // meters
    bool enableRtsCts = true;
    double simulationTime = 10.0; // seconds
    std::string dataRate = "100Mbps"; // Application data rate
    uint32_t packetSize = 1472; // bytes (1500 - UDP/IP header size)

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Distance between AP and STA in meters", distance);
    cmd.AddValue("enableRtsCts", "Enable/Disable RTS/CTS (true/false)", enableRtsCts);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Application data rate (e.g., 100Mbps)", dataRate);
    cmd.AddValue("packetSize", "Application packet size in bytes", packetSize);
    cmd.Parse(argc, argv);

    // Initialize global max TXOP durations vector for 4 networks
    g_maxTxopDurations.resize(4, Time(0));

    // Configure RTS/CTS threshold globally for all WifiMacs
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(enableRtsCts ? "1" : "999999"));
    // Disable fragmentation by setting threshold to a large value
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("999999"));

    NodeContainer apNodes[4];
    NodeContainer staNodes[4];
    NetDeviceContainer apDevices[4];
    NetDeviceContainer staDevices[4];
    Ipv4InterfaceContainer apInterfaces[4];
    Ipv4InterfaceContainer staInterfaces[4];

    std::vector<int> channels = {36, 40, 44, 48}; // 5 GHz band channels
    std::string ssids[] = {"ns3-wifi-n-agg-1", "ns3-wifi-n-agg-2", "ns3-wifi-n-agg-3", "ns3-wifi-n-agg-4"};

    for (int i = 0; i < 4; ++i)
    {
        NS_LOG_INFO("Setting up Network " << (i + 1) << " on Channel " << channels[i]);

        // Create AP and STA nodes
        apNodes[i].Create(1);
        staNodes[i].Create(1);

        // Configure Mobility
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // AP at origin
        positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA at 'distance' away
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNodes[i]);
        mobility.Install(staNodes[i]);

        // Configure Wi-Fi physical layer and channel
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
        // Use IdealWifiManager for controlled throughput and to observe aggregation effects clearly
        wifi.SetRemoteStationManager("ns3::IdealWifiManager");

        YansWifiPhyHelper phy;
        phy.SetChannel(YansWifiChannelHelper::Create());
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.Set("ChannelNumber", UintegerValue(channels[i]));

        // Configure STA Wi-Fi Mac
        WifiMacHelper mac;
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssids[i]),
                    "ActiveProbing", BooleanValue(false)); // STA is passive listener
        staDevices[i] = wifi.Install(phy, mac, staNodes[i]);

        // Configure AP Wi-Fi Mac
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssids[i]),
                    "BeaconInterval", TimeValue(MicroSeconds(102400))); // Default beacon interval
        apDevices[i] = wifi.Install(phy, mac, apNodes[i]);

        // Retrieve STA's WifiMac to set specific aggregation attributes
        Ptr<WifiNetDevice> staWifiDevice = DynamicCast<WifiNetDevice> (staDevices[i].Get(0));
        Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac> (staWifiDevice->GetMac());

        if (i == 0) // STA A: Default A-MPDU (65kB), A-MSDU disabled
        {
            NS_LOG_INFO("  Network 1 (STA A): Default A-MPDU (Max 65kB), A-MSDU Disabled");
            staMac->SetAttribute("EnableAmpdu", BooleanValue(true)); // Explicitly enable (is default for 802.11n)
            staMac->SetAttribute("MaxAmpduSize", UintegerValue(65535));
            staMac->SetAttribute("EnableAmdu", BooleanValue(false)); // Explicitly disable (is default for 802.11n)
        }
        else if (i == 1) // STA B: Aggregation disabled
        {
            NS_LOG_INFO("  Network 2 (STA B): Aggregation Disabled");
            staMac->SetAttribute("EnableAmpdu", BooleanValue(false));
            staMac->SetAttribute("EnableAmdu", BooleanValue(false));
        }
        else if (i == 2) // STA C: A-MSDU enabled (8kB), A-MPDU disabled
        {
            NS_LOG_INFO("  Network 3 (STA C): A-MSDU Enabled (Max 8kB), A-MPDU Disabled");
            staMac->SetAttribute("EnableAmpdu", BooleanValue(false));
            staMac->SetAttribute("EnableAmdu", BooleanValue(true));
            staMac->SetAttribute("MaxAmsduSize", UintegerValue(8 * 1024)); // 8 KB
        }
        else if (i == 3) // STA D: A-MPDU enabled (32kB), A-MSDU enabled (4kB)
        {
            NS_LOG_INFO("  Network 4 (STA D): A-MPDU Enabled (Max 32kB), A-MSDU Enabled (Max 4kB)");
            staMac->SetAttribute("EnableAmpdu", BooleanValue(true));
            staMac->SetAttribute("MaxAmpduSize", UintegerValue(32 * 1024)); // 32 KB
            staMac->SetAttribute("EnableAmdu", BooleanValue(true));
            staMac->SetAttribute("MaxAmsduSize", UintegerValue(4 * 1024)); // 4 KB
        }

        // Install Internet Stack
        InternetStackHelper stack;
        stack.Install(apNodes[i]);
        stack.Install(staNodes[i]);

        // Assign IP addresses
        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);

        // Configure Applications (STA sends UDP traffic to AP)
        uint16_t port = 9000 + i; // Unique port for each network
        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(apNodes[i]);
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(simulationTime));

        UdpClientHelper client(apInterfaces[i].GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0)); // Send indefinitely
        client.SetAttribute("Interval", TimeValue(MicroSeconds(0.001))); // Send as fast as possible
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        client.SetAttribute("RemoteAddress", Ipv4AddressValue(apInterfaces[i].GetAddress(0)));

        ApplicationContainer clientApps = client.Install(staNodes[i]);
        clientApps.Start(Seconds(1.0)); // Start client slightly after server
        clientApps.Stop(Seconds(simulationTime));

        // Trace TXOP Duration for the AP's MacLow layer
        Ptr<WifiNetDevice> apWifiDevice = DynamicCast<WifiNetDevice> (apDevices[i].Get(0));
        Ptr<WifiMac> apMac = apWifiDevice->GetMac();
        Ptr<MacLow> apMacLow = DynamicCast<MacLow> (apMac->GetMacLow());
        apMacLow->TraceConnectWithoutContext("TxopDuration", MakeBoundCallback(&TxopDurationCallback, i));
    }

    // Install FlowMonitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Collect and print results
    monitor->CheckFor     <FlowMonitorSource::PacketTrace> ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.Get;FlowClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->Get;FlowStats();

    std::cout << "\n--- Simulation Results ---" << std::endl;
    std::cout << "Distance: " << distance << "m" << std::endl;
    std::cout << "RTS/CTS: " << (enableRtsCts ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Simulation Time: " << simulationTime << "s" << std::endl;

    for (int i = 0; i < 4; ++i)
    {
        double throughputMbps = 0;
        uint64_t totalBytesReceived = 0;

        for (auto iter = stats.begin(); iter != stats.end(); ++iter)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->GetFlowClassifier()->Find;Flow(iter->first);
            // Check if the flow is from STA to AP for the current network (destination IP and port match AP server)
            if (t.destinationAddress == apInterfaces[i].GetAddress(0) && t.destinationPort == (9000 + i))
            {
                totalBytesReceived += iter->second.rxBytes;
            }
        }
        throughputMbps = (totalBytesReceived * 8.0) / (simulationTime * 1000000.0); // Convert to Mbps

        std::string agg_settings_desc;
        if (i == 0) { agg_settings_desc = "STA A: Default A-MPDU (65kB), A-MSDU Disabled"; }
        else if (i == 1) { agg_settings_desc = "STA B: Aggregation Disabled"; }
        else if (i == 2) { agg_settings_desc = "STA C: A-MSDU Enabled (8kB), A-MPDU Disabled"; }
        else if (i == 3) { agg_settings_desc = "STA D: A-MPDU Enabled (32kB), A-MSDU Enabled (4kB)"; }

        std::cout << "\nNetwork " << (i + 1) << " (Channel " << channels[i] << ", SSID: " << ssids[i] << ")" << std::endl;
        std::cout << "  Aggregation Settings: " << agg_settings_desc << std::endl;
        std::cout << "  Throughput: " << throughputMbps << " Mbps" << std::endl;
        std::cout << "  Max TXOP Duration (AP): " << g_maxTxopDurations[i] << std::endl;
    }

    // Clean up
    monitor->Dispose();
    Simulator::Destroy();

    return 0;
}