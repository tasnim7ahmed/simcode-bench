#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);
    Ptr<Node> apNode = wifiNodes.Get(0);
    Ptr<Node> staNode = wifiNodes.Get(1);

    // WiFi channel & PHY setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // WiFi standard and SSID
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Setup AP
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, apNode);

    // Setup STA
    NetDeviceContainer staDevice;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevice = wifi.Install(phy, mac, staNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));      // AP position
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));      // STA position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer devices;
    devices.Add(staDevice); // [0]: STA
    devices.Add(apDevice);  // [1]: AP
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Server on AP (port 9)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on STA
    uint32_t maxPackets = 100;
    Time interPacketInterval = Seconds(0.1);
    uint32_t packetSize = 512;
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(staNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}