#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Global variables for command-line arguments
static uint32_t g_payloadSize = 1000;      // bytes
static double g_simTime = 10.0;           // seconds
static double g_distance = 5.0;           // meters
static bool g_rtsCts = false;             // RTS/CTS enabled/disabled

int main(int argc, char* argv[])
{
    // Set up time resolution for simulation
    Time::SetResolution(NanoSeconds);

    // Parse command-line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size of the packets (bytes)", g_payloadSize);
    cmd.AddValue("simTime", "Total simulation time (seconds)", g_simTime);
    cmd.AddValue("distance", "Distance between AP and station (meters)", g_distance);
    cmd.AddValue("rtsCts", "Enable or disable RTS/CTS (0 = disable, 1 = enable)", g_rtsCts);
    cmd.Parse(argc, argv);

    // Configure RTS/CTS threshold based on command-line argument
    if (g_rtsCts)
    {
        // Set a small threshold to always trigger RTS/CTS
        Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", UintegerValue(1));
    }
    else
    {
        // Set a very large threshold to effectively disable RTS/CTS
        Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", UintegerValue(999999));
    }

    // Disable fragmentation to prevent it from interfering with aggregation.
    // Set threshold greater than maximum possible aggregated frame size (11n A-MSDU up to 7935, A-MPDU up to 65535).
    // A value of 12000 covers typical A-MSDU frames.
    Config::SetDefault("ns3::WifiMac::FragmentationThreshold", UintegerValue(12000));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(12000));

    // Network setup parameters for each independent network
    const int numNetworks = 4;
    int channels[] = {36, 40, 44, 48}; // 5GHz channels (20 MHz, adjacent in 5.15-5.25 GHz band)

    // Loop to create four independent Wi-Fi networks
    for (int i = 0; i < numNetworks; ++i)
    {
        NS_LOG_INFO("Creating network " << i << " on channel " << channels[i]);

        // Create one AP node and one STA node for the current network
        NodeContainer apNode;
        apNode.Create(1);
        NodeContainer staNode;
        staNode.Create(1);

        // Configure Mobility Model
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

        // AP at origin (0,0,0)
        Ptr<ConstantPositionMobilityModel> apMobility = CreateObject<ConstantPositionMobilityModel>();
        apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
        apNode.Get(0)->SetMobility(apMobility);

        // STA at specified distance (g_distance,0,0)
        Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel>();
        staMobility->SetPosition(Vector(g_distance, 0.0, 0.0));
        staNode.Get(0)->SetMobility(staMobility);

        // Configure Wi-Fi
        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // Using 802.11n for 5GHz

        // Configure Wi-Fi PHY layer
        YansWifiPhyHelper phy;
        phy.SetChannel(YansWifiChannelHelper::Create()); // Create a new channel for each network
        phy.Set("ChannelNumber", UintegerValue(channels[i])); // Assign unique channel number

        // Configure Wi-Fi MAC layer
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi-" + std::to_string(i)); // Unique SSID for each network

        // Set Remote Station Manager (data rate, control rate)
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("OfdmRate54Mbps"),
                                     "ControlMode", StringValue("OfdmRate6Mbps"));

        // Install Wi-Fi devices on nodes
        NetDeviceContainer apDevices;
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
        apDevices = wifi.Install(phy, mac, apNode);

        NetDeviceContainer staDevices;
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices = wifi.Install(phy, mac, staNode);

        // Install Internet Stack on nodes
        InternetStackHelper stack;
        stack.Install(apNode);
        stack.Install(staNode);

        // Assign IP Addresses
        Ipv4AddressHelper address;
        std::string networkAddress = "10." + std::to_string(i) + ".0.0";
        address.SetBase(networkAddress.c_str(), "255.255.255.0");
        Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

        // Configure Aggregation settings specific to each station/network
        Ptr<WifiNetDevice> staWifiNetDevice = DynamicCast<WifiNetDevice>(staDevices.Get(0));
        Ptr<WifiMac> staWifiMac = staWifiNetDevice->GetMac();

        NS_LOG_INFO("Configuring aggregation for network " << i);
        if (i == 0) // Network 0 (Station A, Channel 36)
        {
            // Default aggregation with A-MSDU disabled and A-MPDU enabled at 65 KB
            staWifiMac->SetAttribute("EnableAmsdu", BooleanValue(false));
            staWifiMac->SetAttribute("EnableAmpdu", BooleanValue(true));
            staWifiMac->SetAttribute("MaxAmpduSize", BytesValue(65 * 1024));
            NS_LOG_INFO("Net " << i << ": A-MSDU disabled, A-MPDU enabled (65KB)");
        }
        else if (i == 1) // Network 1 (Station B, Channel 40)
        {
            // Both A-MPDU and A-MSDU disabled
            staWifiMac->SetAttribute("EnableAmsdu", BooleanValue(false));
            staWifiMac->SetAttribute("EnableAmpdu", BooleanValue(false));
            NS_LOG_INFO("Net " << i << ": Both A-MSDU and A-MPDU disabled");
        }
        else if (i == 2) // Network 2 (Station C, Channel 44)
        {
            // A-MSDU enabled with 8 KB and A-MPDU disabled
            staWifiMac->SetAttribute("EnableAmsdu", BooleanValue(true));
            staWifiMac->SetAttribute("MaxAmsduSize", BytesValue(8 * 1024));
            staWifiMac->SetAttribute("EnableAmpdu", BooleanValue(false));
            NS_LOG_INFO("Net " << i << ": A-MSDU enabled (8KB), A-MPDU disabled");
        }
        else if (i == 3) // Network 3 (Station D, Channel 48)
        {
            // Two-level aggregation with A-MPDU at 32 KB and A-MSDU at 4 KB
            staWifiMac->SetAttribute("EnableAmsdu", BooleanValue(true));
            staWifiMac->SetAttribute("MaxAmsduSize", BytesValue(4 * 1024));
            staWifiMac->SetAttribute("EnableAmpdu", BooleanValue(true));
            staWifiMac->SetAttribute("MaxAmpduSize", BytesValue(32 * 1024));
            NS_LOG_INFO("Net " << i << ": A-MSDU enabled (4KB), A-MPDU enabled (32KB)");
        }
        
        // Setup Applications (BulkSend from STA to AP)
        uint16_t port = 9 + i; // Unique port for each network

        // Packet Sink on AP to receive data
        PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = packetSinkHelper.Install(apNode.Get(0));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(g_simTime));

        // BulkSend on STA to continuously transmit data
        BulkSendHelper source("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), port));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // 0 means unlimited bytes (continuous transmission)
        source.SetAttribute("SendRate", DataRateValue(DataRate("100Mbps"))); // High rate to push aggregation
        source.SetAttribute("PacketSize", UintegerValue(g_payloadSize));
        ApplicationContainer sourceApps = source.Install(staNode.Get(0));
        sourceApps.Start(Seconds(1.0)); // Start after 1 second to allow association
        sourceApps.Stop(Seconds(g_simTime));
    }

    // Run Simulation
    Simulator::Stop(Seconds(g_simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}