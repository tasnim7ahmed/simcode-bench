#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetUdpExample");

int main (int argc, char *argv[])
{
    // Enable logging for UdpClient and UdpServer if needed
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Set up Wi-Fi (ad hoc)
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Set random waypoint mobility
    MobilityHelper mobility;
    // Set position allocator: nodes randomly distributed in 100x100 area, z=0
    Ptr<PositionAllocator> positionAlloc = CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
        "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
    );
    mobility.SetPositionAllocator (positionAlloc);

    // Configure RandomWaypointMobilityModel:
    // - speed: constant 20 m/s
    // - pause: 0 s
    // - bounds: 100x100
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=20.0]"),
        "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
        "PositionAllocator", PointerValue (positionAlloc),
        "Bounds", BoxValue (Box (0.0, 100.0, 0.0, 100.0, 0.0, 0.0)));
    mobility.Install (nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // Install UDP server on node 1 (nodes.Get(1)), port 9
    uint16_t serverPort = 9;
    UdpServerHelper udpServer (serverPort);
    ApplicationContainer serverApps = udpServer.Install (nodes.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // Install UDP client on node 0 (nodes.Get(0)), sending to node 1
    uint32_t maxPackets = 1000;
    uint32_t packetSize = 1024;
    double interval = 0.01; // seconds
    UdpClientHelper udpClient (interfaces.GetAddress (1), serverPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = udpClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Run simulation
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}