#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    double simDuration = 10.0;
    double appStart = 2.0;
    uint32_t numNodes = 5;
    double areaWidth = 100.0;
    double areaHeight = 100.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Mobility model: RandomWaypoint for all nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"),
        "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue (
            CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
            )
        )
    );
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
        "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
    );
    mobility.Install(nodes);

    // Wi-Fi: 802.11b, Adhoc
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // UDP server (Receiver) on node 4
    uint16_t udpPort = 9999;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApps = udpServer.Install (nodes.Get(4));
    serverApps.Start (Seconds (appStart));
    serverApps.Stop (Seconds (simDuration));

    // UDP client (Sender) on node 0 targeting node 4
    uint32_t maxPackets = 1000;
    Time interPacketInterval = Seconds (0.05);
    UdpClientHelper udpClient (interfaces.GetAddress (4), udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
    udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    udpClient.SetAttribute ("PacketSize", UintegerValue (512));

    ApplicationContainer clientApps = udpClient.Install (nodes.Get(0));
    clientApps.Start (Seconds (appStart));
    clientApps.Stop (Seconds (simDuration));

    Simulator::Stop (Seconds (simDuration));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}