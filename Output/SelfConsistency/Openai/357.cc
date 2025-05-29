#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set standard output formatting
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes: STA (0) and AP (1)
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up WiFi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // WiFi Standard (e.g. 802.11g for simplicity)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // MAC helpers
    WifiMacHelper mac;

    // Configure the STA
    Ssid ssid = Ssid("ns3-80211g");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Configure the AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set up static positions for nodes
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // STA position
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // AP position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // UDP Echo Server on AP
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);

    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on STA (targeting AP)
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(9.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}