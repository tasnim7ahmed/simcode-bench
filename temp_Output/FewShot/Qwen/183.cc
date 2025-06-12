#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 2 APs and 4 STAs
    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(4);

    // Create channel and configure Wi-Fi for 802.11n
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5MHZ);

    // Install APs
    Ssid ssid1 = Ssid("network-AP1");
    Ssid ssid2 = Ssid("network-AP2");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    // Install STAs - associate with nearest AP automatically
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes.Get(0));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy, mac, staNodes.Get(2));
    NetDeviceContainer staDevices4 = wifi.Install(phy, mac, staNodes.Get(3));

    // Mobility model: place APs and STAs at fixed positions
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    address.NewNetwork();
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces3 = address.Assign(staDevices3);
    Ipv4InterfaceContainer staInterfaces4 = address.Assign(staDevices4);

    // Set up UDP Echo Server on STA3 (connected to AP2)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(staNodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on AP1 sending to STA3
    UdpEchoClientHelper echoClient(staInterfaces3.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(apNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Animation
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-animation.tr"));
    AnimationInterface anim("wifi-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}