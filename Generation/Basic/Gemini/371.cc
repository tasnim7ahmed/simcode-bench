#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
    Time::SetResolution (NanoSeconds);

    NodeContainer nodes;
    nodes.Create (2);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211a);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel;
    channel.SetPropagationLoss ("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer wifiDevices;
    wifiDevices = wifi.Install (phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (1.0),
                                   "DeltaY", DoubleValue (0.0),
                                   "GridWidth", UintegerValue (2),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (wifiDevices);

    uint16_t port = 9;

    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (nodes.Get (1));
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (10.0));

    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (1000));
    client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = client.Install (nodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (9.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}