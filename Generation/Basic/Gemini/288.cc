#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h" // Not strictly needed, but common

using namespace ns3;

int
main(int argc, char *argv[])
{
    double simulationTime = 10.0; // seconds

    // Configure command line arguments
    CommandLine cmd;
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Disable fragmentation for simplicity and higher throughput with 11ac
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("999999"));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));

    // 1. Create nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    // 2. Configure Mobility
    // AP: Stationary at (0,0,0)
    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNode);
    apNode.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // STAs: Random movement within 50x50 area
    MobilityHelper staMobility;
    staMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]")); // 5 m/s speed
    staMobility.Install(staNodes);

    // 3. Configure Wi-Fi (802.11ac)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5.18e9)); // 5.18 GHz for 11ac

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set a consistent channel number for 5GHz
    phy.Set("ChannelNumber", UintegerValue(36));

    // VHT (802.11ac) MAC helper
    VhtWifiMacHelper vhtMac = VhtWifiMacHelper::Default();

    Ssid ssid = Ssid("ns3-wifi-ac");

    // Configure STA MAC
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("VhtMcsIndex6"), // Example 11ac data rate (MCS 6 for 20MHz bandwidth)
                                 "ControlMode", StringValue("VhtMcsIndex0")); // Control rate (MCS 0)
    vhtMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, vhtMac, staNodes);

    // Configure AP MAC
    vhtMac.SetType("ns3::ApWifiMac",
                   "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, vhtMac, apNode);

    // 4. Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 6. Setup Applications
    // STA 0 acts as TCP server on port 5000
    uint16_t port = 5000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sinkHelper.Install(staNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1.0)); // Run a bit longer than client

    // STA 1 acts as TCP client, sending 1024-byte packets at 100Mbps
    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port)); // Connect to STA 0
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", DataRateValue(100 * 1000 * 1000)); // 100 Mbps

    ApplicationContainer clientApp = onoff.Install(staNodes.Get(1));
    clientApp.Start(Seconds(1.0)); // Client starts after server
    clientApp.Stop(Seconds(simulationTime));

    // 7. Enable PCAP tracing
    phy.EnablePcap("wifi-ac-simulation-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-ac-simulation-sta0", staDevices.Get(0));
    phy.EnablePcap("wifi-ac-simulation-sta1", staDevices.Get(1));

    // 8. Run simulation
    Simulator::Stop(Seconds(simulationTime + 2.0)); // Ensure all applications and cleanup finish
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}