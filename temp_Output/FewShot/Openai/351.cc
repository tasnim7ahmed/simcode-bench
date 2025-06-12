#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for debugging (optional, can enable as needed)
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Configure Wi-Fi Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi-ssid");

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

    // STA (client) configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

    NetDeviceContainer wifiDevices;
    wifiDevices.Add(apDevices);
    wifiDevices.Add(staDevices);

    // Mobility - both nodes have constant position mobility model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // AP position
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));  // Client position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(wifiDevices);

    // AP is node 0, Client is node 1
    Ipv4Address apAddress = Ipv4Address("192.168.1.1");
    Ipv4Address clientAddress = interfaces.GetAddress(1);

    // Ensure AP gets 192.168.1.1
    Ptr<Ipv4> ipv4Ap = wifiNodes.Get(0)->GetObject<Ipv4>();
    ipv4Ap->SetMetric(1, 1);
    ipv4Ap->SetUp(1);
    ipv4Ap->SetForwarding(1, true);
    ipv4Ap->SetRoutingProtocol(0);

    ipv4Ap->SetAddress(1, Ipv4InterfaceAddress(apAddress, Ipv4Mask("255.255.255.0")));

    // UDP server on AP (node 0), port 8080
    uint16_t port = 8080;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on node 1 (client)
    UdpClientHelper udpClient(apAddress, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(20));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}