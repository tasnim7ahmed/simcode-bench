#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeWifiForwardingExample");

int main (int argc, char *argv[])
{
    // Enable logging for debugging as needed
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Set simulation time
    double simulationTime = 10.0;

    // Create nodes: 0 (STA), 1 (AP), 2 (forwarding node)
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (1); // Node 0

    NodeContainer wifiApNode;
    wifiApNode.Create (1); // Node 1 (AP)

    NodeContainer csmaNode;
    csmaNode.Create(1); // Node 2 (forwarding node)

    // For clarity in code, assign indices
    Ptr<Node> staNode = wifiStaNodes.Get(0);
    Ptr<Node> apNode = wifiApNode.Get(0);
    Ptr<Node> node3 = csmaNode.Get(0);

    // Create Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

    // Use AARF rate control
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    WifiMacHelper mac;

    // Set up SSID
    Ssid ssid = Ssid ("ns3-wifi-ssid");

    // Configure STA device
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    // Configure AP device
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);

    // Mobility for Wi-Fi nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    // Place STA at (0,0,0), AP at (5,0,0)
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (5.0),
                                  "DeltaY", DoubleValue (0.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.Install (wifiStaNodes);
    mobility.Install (wifiApNode);

    // Place node3 at (10,0,0)
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
    posAlloc->Add (Vector (10.0, 0.0, 0.0));
    mobility.SetPositionAllocator(posAlloc);
    mobility.Install(csmaNode);

    // Connect AP (node 1) and node 3 via CSMA
    NodeContainer csmaNodes;
    csmaNodes.Add (apNode);
    csmaNodes.Add (node3);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install (csmaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (wifiStaNodes);
    stack.Install (wifiApNode);
    stack.Install (csmaNode);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // WiFi network
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign (staDevices);
    apInterface = address.Assign (apDevices);

    // CSMA network
    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apCsmaInterface, node3CsmaInterface;
    apCsmaInterface = address.Assign (NetDeviceContainer (csmaDevices.Get (0)));
    node3CsmaInterface = address.Assign (NetDeviceContainer (csmaDevices.Get (1)));

    // Set up static routing on AP to forward packets to node 3
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> apIpv4 = apNode->GetObject<Ipv4> ();
    Ptr<Ipv4StaticRouting> apStaticRouting = ipv4RoutingHelper.GetStaticRouting (apIpv4);

    // Route for packets to 10.1.2.0/24 (to node 3) via CSMA interface
    apStaticRouting->AddNetworkRouteTo (
        Ipv4Address ("10.1.2.0"),
        Ipv4Mask ("255.255.255.0"),
        apIpv4->GetInterfaceForDevice (csmaDevices.Get (0)));

    // Node 1's (AP) routing table already allows connected subnet (Wi-Fi).
    // Enable IP forwarding (by default, SetForwarding for interface 1,2 to true - ns-3 does it at InternetStackHelper::Install)
    apIpv4->SetAttribute ("IpForward", BooleanValue (true));

    // On node 3, just listen for UDP traffic.
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer (echoPort);

    ApplicationContainer serverApps = echoServer.Install (node3);
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simulationTime));

    // On node 0, send UDP packets to node 3
    UdpEchoClientHelper echoClient (node3CsmaInterface.GetAddress (0), echoPort);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = echoClient.Install (staNode);
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simulationTime));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}