#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpExample");

int main(int argc, char *argv[])
{
    // Create nodes
    NodeContainer ap, sta;
    ap.Create(1);
    sta.Create(1);

    // Install Wi-Fi devices on the nodes
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("wifi-network")));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, ap);
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, sta);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ap);
    internet.Install(sta);

    // Set up mobility model for nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ap);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(sta);

    // Assign IP addresses to devices
    Ipv4AddressHelper ipv4Address;
    ipv4Address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIpIface = ipv4Address.Assign(apDevice);
    Ipv4InterfaceContainer staIpIface = ipv4Address.Assign(staDevice);

    // Set up UDP client application on Node 2 (STA)
    uint16_t port = 9;
    UdpClientHelper udpClient(staIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(ap.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Set up UDP server application on Node 1 (AP)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(sta.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
