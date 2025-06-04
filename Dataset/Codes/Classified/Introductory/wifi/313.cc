#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleWifiExample");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 2 stations (STA) and 1 access point (AP)
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(2);
    wifiApNode.Create(1);

    // Set up Wi-Fi devices (STA and AP)
    WifiHelper wifi;
    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-ssid");

    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Install Wi-Fi devices on stations
    NetDeviceContainer staDevices = wifi.Install(wifiMac, wifi, wifiStaNodes);

    // Install Wi-Fi devices on the access point
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(wifiMac, wifi, wifiApNode);

    // Mobility model: Fixed position for AP, random for STA
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator", "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"), "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Time", StringValue("1s"), "Distance", StringValue("10m"));
    mobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(wifiStaNodes);
    internet.Install(wifiApNode);

    // Assign IP addresses to all devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);

    // Set up a UDP server on STA 0
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up a UDP client on STA 1
    UdpClientHelper udpClient(apInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
