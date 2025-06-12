#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeAdhocAodv");

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Log levels (optional)
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create (3);

    // Configure Wi-Fi 802.11b
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Mobility: linear layout, 10m apart
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (10.0, 0.0, 0.0));
    positionAlloc->Add (Vector (20.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Internet stack and AODV routing
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper (aodv);
    stack.Install (nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // UDP Echo Server on Node 2 (last node)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer (echoPort);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // UDP Echo Client on Node 0 (first node) to Node 2
    UdpEchoClientHelper echoClient (interfaces.GetAddress (2), echoPort);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}