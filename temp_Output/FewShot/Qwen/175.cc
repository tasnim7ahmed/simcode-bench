#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for Wi-Fi
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes: one as AP and two as STAs
    NodeContainer wifiNodes;
    wifiNodes.Create(3);

    // Select node 0 as the access point
    Ptr<Node> apNode = wifiNodes.Get(0);
    NodeContainer staNodes;
    staNodes.Add(wifiNodes.Get(1));
    staNodes.Add(wifiNodes.Get(2));

    // Set up Wi-Fi channel and PHY layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set Wi-Fi standard (802.11n)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5MHZ);

    // Set up MAC and install on AP and STAs
    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-network");

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)),
                "BeaconJitter", UintegerValue(0));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Configure STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Set up mobility model (constant position for AP and random for STAs)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Set up UDP Echo Server on the AP (node 0)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(apNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on both STAs sending to AP
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient.Install(staNodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    ApplicationContainer clientApp2 = echoClient.Install(staNodes.Get(1));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(10.0));

    // Enable NetAnim visualization
    AnimationInterface anim("wifi_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}