#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Disable fragmentation for all stations
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
    // Disable RTS/CTS
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

    // Create Wifi helper and set standard to 802.11g
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    // Set up MAC layer
    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));

    // Install PHY and Devices
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, NodeContainer::GetGlobal()->Create(2));

    // Setup AP node
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, NodeContainer::GetGlobal()->Get(0)); // Use first created node as AP

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevice, staDevices));

    // Station 1 (node index 1) acts as UDP server on port 8080
    UdpEchoServerHelper echoServer(8080);
    ApplicationContainer serverApp = echoServer.Install(NodeContainer::GetGlobal()->Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Station 0 (node index 0) acts as UDP client sending packets to server
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 8080); // Server's IP is 192.168.1.3
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(NodeContainer::GetGlobal()->Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}