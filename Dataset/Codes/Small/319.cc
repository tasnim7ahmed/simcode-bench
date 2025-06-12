#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleWifiExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("SimpleWifiExample", LOG_LEVEL_INFO);

    // Create nodes: one Access Point (AP) and multiple stations (STA)
    NodeContainer apNode, staNodes;
    apNode.Create(1);
    staNodes.Create(3);

    // Set up Wi-Fi helper
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Set up Wi-Fi PHY and MAC layer
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    // Install devices (Wi-Fi NICs) on nodes
    NetDeviceContainer apDevice, staDevices;
    apDevice = wifi.Install(wifiPhy, apMac, apNode);
    staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);

    // Set up UDP server on the AP node
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on the stations
    UdpClientHelper udpClient(apInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(staNodes);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
