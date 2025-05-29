#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    NodeContainer nodes;
    nodes.Create (2);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211a);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, nodes.Get(0));

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, nodes.Get(1));

    NetDeviceContainer devices;
    devices.Add (staDevices);
    devices.Add (apDevices);

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::GridPositionAllocator",
                               "MinX", DoubleValue (0.0),
                               "MinY", DoubleValue (0.0),
                               "DeltaX", DoubleValue (5.0),
                               "DeltaY", DoubleValue (0.0),
                               "GridWidth", UintegerValue (2),
                               "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get(1));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (10.0));

    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (320));
    client.SetAttribute ("Interval", TimeValue (Seconds (0.03125))); // 32 packets/sec
    client.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApp = client.Install (nodes.Get(0));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}