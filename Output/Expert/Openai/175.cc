#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 2 STAs + 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Set up Wi-Fi channel and PHY layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up Wi-Fi MAC and standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac); // Modern WiFi standard
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi-demo");

    // Set up STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Set up AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // STA 0
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));    // STA 1
    positionAlloc->Add(Vector(2.5, 5.0, 0.0));    // AP

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    address.Assign(apDevice);

    // UDP Echo Server on STA 0
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on STA 1
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // NetAnim
    AnimationInterface anim("wifi-three-node.xml");
    anim.SetConstantPosition(wifiStaNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(wifiStaNodes.Get(1), 5.0, 0.0);
    anim.SetConstantPosition(wifiApNode.Get(0), 2.5, 5.0);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}