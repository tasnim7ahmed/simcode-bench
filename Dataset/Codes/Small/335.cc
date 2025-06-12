#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("WifiTcpExample", LOG_LEVEL_INFO);

    // Create nodes for Client and Server
    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1); // Client (STA)
    wifiApNode.Create(1);  // Access Point (AP)

    // Install Wi-Fi devices on both the Client and AP
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // 802.11n standard

    WifiMacHelper mac;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-ssid")));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNode);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-ssid")));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

    // Set up the mobility model for the STA
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
    mobility.Install(wifiStaNode); // Install mobility on the STA node
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode); // Install stationary AP

    // Install the internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

    // Set up TCP server on AP
    TcpServerHelper tcpServer(9); // Listening on port 9
    ApplicationContainer serverApp = tcpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on STA
    TcpClientHelper tcpClient(apInterfaces.GetAddress(0), 9);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(10));     // 10 packets to send
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1 second interval
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets
    ApplicationContainer clientApp = tcpClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
