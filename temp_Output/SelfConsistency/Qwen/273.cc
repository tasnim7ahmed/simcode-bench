#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInfrastructureSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create AP and STA nodes
    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(3);

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    // Disable rate control for simplicity
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    // Setup PHY and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiChannelHelper channel = WifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Setup MAC layer
    Ssid ssid = Ssid("wifi-infra");
    WifiMacHelper mac;

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Configure STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    // Mobility models for STAs (random)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(staNodes);

    // Stationary AP
    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Set up UDP echo server on AP
    UdpEchoServerHelper echoServer(9); // port 9
    ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP echo client on one STA (e.g., first STA)
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcap("wifi_infra_ap", apDevice.Get(0));
    phy.EnablePcap("wifi_infra_sta0", staDevices.Get(0));
    phy.EnablePcap("wifi_infra_sta1", staDevices.Get(1));
    phy.EnablePcap("wifi_infra_sta2", staDevices.Get(2));

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}