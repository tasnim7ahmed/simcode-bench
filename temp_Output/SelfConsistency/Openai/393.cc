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
    Time::SetResolution (Time::NS);

    // Create 2 nodes
    NodeContainer wifiNodes;
    wifiNodes.Create (2);

    // Set up Wi-Fi PHY and MAC
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid ("simple-wifi-ssid");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiNodes.Get (0));

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiNodes.Get (1));

    // Mobility: Constant position for both nodes
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));   // Node 0
    positionAlloc->Add (Vector (5.0, 0.0, 0.0));   // Node 1 (AP) within Wi-Fi range

    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install (wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add (address.Assign (staDevices));
    interfaces.Add (address.Assign (apDevices));

    // UDP server (sink) on node 1
    uint16_t port = 4000;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (wifiNodes.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // UDP client on node 0
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 10;
    Time interPacketInterval = Seconds (1.0);

    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}