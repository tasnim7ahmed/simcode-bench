#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Wi-Fi (ad hoc)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set up mobility (RandomWaypoint within 100x100 area, constant 20 m/s)
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> posX = CreateObject<UniformRandomVariable>();
    posX->SetAttribute("Min", DoubleValue(0.0));
    posX->SetAttribute("Max", DoubleValue(100.0));
    Ptr<UniformRandomVariable> posY = CreateObject<UniformRandomVariable>();
    posY->SetAttribute("Min", DoubleValue(0.0));
    posY->SetAttribute("Max", DoubleValue(100.0));

    ObjectFactory posFactory;
    posFactory.SetTypeId("ns3::RandomRectanglePositionAllocator");
    posFactory.Set("X", PointerValue(posX));
    posFactory.Set("Y", PointerValue(posY));

    Ptr<PositionAllocator> positionAlloc = posFactory.Create()->GetObject<PositionAllocator>();

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
        "PositionAllocator", PointerValue(positionAlloc));

    mobility.SetPositionAllocator(positionAlloc);

    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP server
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}