#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set simulation time and other parameters
    double simulationTime = 20.0;
    uint32_t packetSize = 512;
    uint32_t numPackets = 100;
    double interval = 0.1; // seconds
    uint16_t port = 9;

    // Create nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Install WiFi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid ("ns3-80211b");

    // Install device on both nodes (set one as AP and one as STA)
    wifiMac.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install (wifiPhy, wifiMac, nodes.Get (0));

    wifiMac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (wifiPhy, wifiMac, nodes.Get (1));

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));   // node 0
    positionAlloc->Add (Vector (5.0, 10.0, 0.0));  // node 1
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign (NetDeviceContainer (staDevice, apDevice));

    // Install server on node 1
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                       InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (1));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (simulationTime));

    // Install TCP client on node 0
    OnOffHelper clientHelper ("ns3::TcpSocketFactory",
                              InetSocketAddress (interfaces.GetAddress (1), port));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    clientHelper.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
    clientHelper.SetAttribute ("DataRate", StringValue ("40960bps")); // just higher than 512*8/0.1=40960bps
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute ("Interval", TimeValue (Seconds (interval)));

    ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (simulationTime));

    // Run simulation
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}