#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    NodeContainer nodes;
    nodes.Create (5);

    // Wifi
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"), "ControlMode", StringValue ("DsssRate1Mbps"));

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"),
        "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
        "PositionAllocator", PointerValue (
            CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
            ))
    );
    mobility.Install (nodes);

    // Internet stack and AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv);
    internet.Install (nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // UDP Server on node 4
    uint16_t port = 4000;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (nodes.Get (4));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (20.0));

    // UDP Client on node 0
    UdpClientHelper client (interfaces.GetAddress (4), port);
    client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
    client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
    client.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApp = client.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (20.0));

    // Enable tracing
    wifiPhy.EnablePcap ("manet-aodv", devices);

    Simulator::Stop (Seconds (20.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}