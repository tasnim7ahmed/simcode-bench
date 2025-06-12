#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set the simulation time resolution
    Time::SetResolution(Time::NS);

    // Create nodes: one Access Point (AP) and three Client Stations (STA)
    NodeContainer apNode, staNodes;
    apNode.Create(1);
    staNodes.Create(3);

    // Set up the Wi-Fi helper
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    // Install Wi-Fi devices on the AP and STAs
    NetDeviceContainer apDevice, staDevices;
    apDevice = wifiHelper.Install(phy, mac, apNode);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    staDevices = wifiHelper.Install(phy, mac, staNodes);

    // Install IP stack for routing
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIpIface, staIpIface;
    apIpIface = ipv4.Assign(apDevice);
    staIpIface = ipv4.Assign(staDevices);

    // Set up a UDP server on the Access Point (AP)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP clients on the Stations (STA)
    UdpClientHelper udpClient(apIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(5));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 3; ++i)
    {
        clientApps.Add(udpClient.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set mobility model for AP and STAs
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(staNodes);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
