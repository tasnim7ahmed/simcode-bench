#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
    // Set simulation time
    double simulationTime = 10.0; // seconds

    // Enable logging for UdpClient and UdpServer if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer wifiNodes;
    wifiNodes.Create (2);

    // Set up Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    // Set up Wi-Fi helper and AARF manager
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    // Set up MAC & install devices
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
    Ssid ssid = Ssid ("simple-wifi");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install (phy, mac, wifiNodes.Get (0));

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiNodes.Get (1));

    NetDeviceContainer devices;
    devices.Add (staDevice);
    devices.Add (apDevice);

    // Mobility: set static positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));    // Node 0 (sender)
    positionAlloc->Add (Vector (5.0, 5.0, 0.0));    // Node 1 (receiver)
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Install UDP server on Node 1
    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (wifiNodes.Get (1));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (simulationTime));

    // Install UDP client on Node 0
    uint32_t packetSize = 1024; // bytes
    uint32_t maxPacketCount = 4294967295; // practically unlimited
    Time interPacketInterval = MilliSeconds (1);
    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (simulationTime));

    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}