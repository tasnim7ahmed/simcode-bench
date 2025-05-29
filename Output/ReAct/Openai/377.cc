#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create nodes: 0 - station, 1 - AP, 2 - "wired" node
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (1);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);
    NodeContainer wiredNode;
    wiredNode.Create (1);

    // Wi-Fi helpers/configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi");

    NetDeviceContainer staDevices;
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    apDevices = wifi.Install (phy, mac, wifiApNode);

    // Set up CSMA (wired) between AP and Node 2
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
    NodeContainer csmaNodes;
    csmaNodes.Add (wifiApNode.Get (0));
    csmaNodes.Add (wiredNode.Get (0));
    NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (5.0),
                                   "DeltaY", DoubleValue (10.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes);
    mobility.Install (wifiApNode);
    mobility.Install (wiredNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (wifiStaNodes);
    stack.Install (wifiApNode);
    stack.Install (wiredNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfs, apIfs;
    staIfs = address.Assign (staDevices);
    apIfs = address.Assign (apDevices);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaIfs = address.Assign (csmaDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // UDP Server on Node 2 (wired node)
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApp = udpServer.Install (wiredNode.Get (0));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // UDP Client on Node 0 (station)
    UdpClientHelper udpClient (csmaIfs.GetAddress (1), udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (320));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.05)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}