#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMobilityExample");

int main(int argc, char* argv[]) {
    bool enablePcap = true;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Disable fragmentation at the MAC layer
    UintegerValue fragSize = UintegerValue(1500);
    Config::SetDefault("ns3::WifiMacHeader::FragmentThreshold", fragSize);
    Config::SetDefault("ns3::WifiMacHeader::RtsCtsThreshold", UintegerValue::Max());

    // Create nodes: AP and two STAs
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    // Configure the PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure the MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

    WifiMacHelper mac;

    // Configure the AP
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid),
        "BeaconGeneration", BooleanValue(true),
        "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Configure the STAs
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Mobility model: Random walk 2D for STAs, fixed for AP
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
        "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Mode", StringValue("Time"),
        "Time", StringValue("1s"),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
        "Bounds", StringValue("50x50"));
    mobility.Install(staNodes);

    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNode);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

    // Create a TCP server on one STA
    uint16_t port = 5000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(staNodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create a TCP client on the other STA
    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable PCAP tracing
    if (enablePcap) {
        phy.EnablePcap("wifi-mobility", apDevice);
        phy.EnablePcap("wifi-mobility", staDevices);
    }

    // Enable FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim("wifi-mobility.xml");
    anim.SetConstantPosition(apNode.Get(0), 25, 25);

    Simulator::Run();

    // Print FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Iprobe> probe = monitor->GetIprobe();

    Simulator::Destroy();

    return 0;
}