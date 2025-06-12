#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Wifi settings
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=20.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                              "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
    mobility.Install (nodes);

    // Internet stack and IP addresses
    InternetStackHelper internet;
    internet.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // UDP Server on node 1
    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (nodes.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // UDP Client on node 0 sending to node 1
    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (1000));
    client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = client.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}