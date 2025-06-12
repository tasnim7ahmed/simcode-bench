#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleWifiExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("SimpleWifiExample", LOG_LEVEL_INFO);

    // Create nodes: Station (STA) and Access Point (AP)
    NodeContainer staNodes, apNodes;
    staNodes.Create(1);
    apNodes.Create(1);

    // Set up WiFi helper and configure WiFi standards
    WifiHelper wifiHelper;
    wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

    // Set up WiFi channel and PHY layers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up MAC layer with IEEE 802.11 standards
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install(phy, mac, staNodes);

    // Set up AP MAC layer
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifiHelper.Install(phy, mac, apNodes);

    // Install the internet stack (IP, TCP, UDP)
    InternetStackHelper internet;
    internet.Install(staNodes);
    internet.Install(apNodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staIpIface = ipv4.Assign(staDevices);
    Ipv4InterfaceContainer apIpIface = ipv4.Assign(apDevices);

    // Set up UDP server on the AP node (Access Point)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(apNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on the STA node (Station)
    UdpClientHelper udpClient(apIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
