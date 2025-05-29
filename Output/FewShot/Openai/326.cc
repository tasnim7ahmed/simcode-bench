#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for UDP applications if desired
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t numNodes = 5;
    double simTime = 10.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Wi-Fi 802.11b
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypointMobilityModel within 100x100m area
    MobilityHelper mobility;
    ObjectFactory pos;
    pos.SetTypeId("ns3::UniformRandomVariable");
    pos.Set("Min", DoubleValue(0.0));
    pos.Set("Max", DoubleValue(100.0));

    Ptr<PositionAllocator> positionAlloc = CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetAttribute("X", RandomVariableValue(UniformVariable(0.0, 100.0)));
    positionAlloc->SetAttribute("Y", RandomVariableValue(UniformVariable(0.0, 100.0)));

    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue(positionAlloc)
    );
    mobility.Install(nodes);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP Echo Server on node 4
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(simTime));

    // Set up UDP Echo Client on node 0 targeting node 4
    UdpEchoClientHelper echoClient(interfaces.GetAddress(4), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}