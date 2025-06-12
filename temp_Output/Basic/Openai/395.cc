#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Configure WiFi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid ("wifi-network");
    wifiMac.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevices, apDevices;
    staDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (0));

    wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
    apDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (1));

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (5.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    NetDeviceContainer devices;
    devices.Add (staDevices.Get (0));
    devices.Add (apDevices.Get (0));
    interfaces = address.Assign (devices);

    // UDP Server on node 1
    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (nodes.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (12.0));

    // UDP Client on node 0
    uint32_t maxPackets = 1000;
    Time interPacketInterval = MilliSeconds (10);
    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = client.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (12.0));

    Simulator::Stop (Seconds (12.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}