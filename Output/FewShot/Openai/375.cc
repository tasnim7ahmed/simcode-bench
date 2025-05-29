#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for applications
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: 0 - AP, 1 - STA
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Wifi PHY and channel setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wifi MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;

    // Configure SSID
    Ssid ssid = Ssid("ns3-ssid");

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Mobility
    MobilityHelper mobility;
    // AP is stationary at (5,5,0)
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(5.0, 5.0, 0.0)); // AP
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // STA initial position
    mobility.SetPositionAllocator(positionAlloc);

    // AP static
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes.Get(0));

    // STA random walk within rectangle, 0<=x<=10, 0<=y<=10
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 10.0, 0.0, 10.0)),
        "Distance", DoubleValue(2.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(wifiNodes.Get(1));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP address assignment
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer devices;
    devices.Add(apDevice);
    devices.Add(staDevice);
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server (on AP, node 0)
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client (on STA, node 1)
    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(wifiNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}