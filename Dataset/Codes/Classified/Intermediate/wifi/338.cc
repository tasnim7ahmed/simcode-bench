#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MobileWifiUdpExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("MobileWifiUdpExample", LOG_LEVEL_INFO);

    // Create nodes: two mobile devices and one access point (AP)
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(2); // Two mobile devices
    wifiApNode.Create(1);   // One access point

    // Install Wi-Fi devices on the nodes
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-ssid")));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-ssid")));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

    // Set mobility for the sender and receiver (random walk in a 100m x 100m area)
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
    mobility.Install(wifiStaNodes); // Install mobility on the mobile devices
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode); // Install constant position for AP

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

    // Set up UDP server on the access point
    UdpServerHelper udpServer(9); // Listening on port 9
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on the mobile devices
    UdpClientHelper udpClient(apInterfaces.GetAddress(0), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Send 10 packets
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets

    ApplicationContainer clientApp1 = udpClient.Install(wifiStaNodes.Get(0)); // First mobile device
    ApplicationContainer clientApp2 = udpClient.Install(wifiStaNodes.Get(1)); // Second mobile device
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
