#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set time resolution and logging
    Time::SetResolution(Time::NS);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Configure WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set mobility: Random Walk 2D Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
        "Time", StringValue("ns3::ConstantRandomVariable[Constant=0.5]")
    );
    mobility.SetPositionAllocator(
        "ns3::UniformDiscPositionAllocator",
        "X", DoubleValue(50.0),
        "Y", DoubleValue(50.0),
        "Rho", DoubleValue(35.0)
    );
    mobility.Install(nodes);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP Echo Server on node 4
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(4), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable static routing for ARP resolution
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}