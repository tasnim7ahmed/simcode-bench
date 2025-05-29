#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshExample");

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (3);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (40.0, 0.0, 0.0));
    positionAlloc->Add (Vector (80.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    // Wi-Fi PHY and Channel
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());

    // Mesh configuration
    MeshHelper mesh = MeshHelper::Default ();
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
    mesh.SetStandard (WIFI_STANDARD_80211s);

    NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

    // Install Internet stack
    InternetStackHelper internetStack;
    internetStack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

    // UDP Server on Node 3
    uint16_t serverPort = 50000;
    UdpServerHelper server (serverPort);
    ApplicationContainer serverApps = server.Install (nodes.Get (2));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // UDP Forwarder (Node 2 receives packets, forwards to Node 3)
    // But since ns-3 does not have an out-of-the-box UDP forwarder, we do a 2-step: Node 1 sends to Node 2, and Node 2 runs a client forwarding to Node 3

    // UDP Server on Node 2 (for forwarding)
    uint16_t intermediatePort = 50001;
    UdpServerHelper intermediateServer (intermediatePort);
    ApplicationContainer intermediateServerApps = intermediateServer.Install (nodes.Get (1));
    intermediateServerApps.Start (Seconds (1.0));
    intermediateServerApps.Stop (Seconds (10.0));

    // UDP Client on Node 1: send to Node 2 (which acts as forwarding agent)
    uint32_t maxPacketCount = 10;
    Time interPacketInterval = Seconds (1.0);

    UdpClientHelper clientToNode2 (interfaces.GetAddress (1), intermediatePort);
    clientToNode2.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    clientToNode2.SetAttribute ("Interval", TimeValue (interPacketInterval));
    clientToNode2.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = clientToNode2.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // UDP Client on Node 2: forward any received packets to Node 3
    // Since ns-3 UdpServer does not spawn an event on packet reception for forwarding,
    // we mimic forwarding with a periodic client for demonstration purposes.
    UdpClientHelper forwardClient (interfaces.GetAddress (2), serverPort);
    forwardClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
    forwardClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    forwardClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer forwardApps = forwardClient.Install (nodes.Get (1));
    forwardApps.Start (Seconds (2.1));
    forwardApps.Stop (Seconds (10.0));

    // Run simulation
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}