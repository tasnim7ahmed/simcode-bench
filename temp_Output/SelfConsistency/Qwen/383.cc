#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/application-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleStationToAp");

int main(int argc, char *argv[])
{
    // Disable fragmentation for simplification
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("999999"));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));

    // Create nodes: Node 0 is the AP, Node 1 is the station
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Create channel and devices
    WifiHelper wifi;
    WifiMacHelper mac;
    WifiPhyHelper phy;
    phy.Set("ChannelWidth", UintegerValue(20));

    // Setup the AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // Setup the STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns-3-ssid")));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Place nodes at fixed positions (not required but helps in visualization)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // Setup UDP server on the AP (Node 0)
    UdpServerHelper server(4000); // port 4000
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP client on the station (Node 1), sending to the AP
    UdpClientHelper client(apInterface.GetAddress(0), 4000);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295)); // unlimited packets
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));     // send every 0.1s
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}