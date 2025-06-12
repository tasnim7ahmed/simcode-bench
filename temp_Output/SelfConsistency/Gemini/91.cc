#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiInterfaceRouting");

// Callback function for packet reception at the source node
static void
RxCallbackSource (Ptr<const Packet> p, const Address &addr)
{
  NS_LOG_INFO ("Received packet at source node: " << p->GetSize () << " bytes from " << addr);
}

// Callback function for packet reception at the destination node
static void
RxCallbackDestination (Ptr<const Packet> p, const Address &addr)
{
  NS_LOG_INFO ("Received packet at destination node: " << p->GetSize () << " bytes from " << addr);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLevel (LogComponent::GetComponent ("UdpClient"), LOG_LEVEL_INFO);
  LogComponent::SetLevel (LogComponent::GetComponent ("UdpServer"), LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (6); // Source, Dest, Router1, Router2, ExtraNode1, ExtraNode2

  NodeContainer sourceNode = NodeContainer (nodes.Get (0));
  NodeContainer destNode = NodeContainer (nodes.Get (1));
  NodeContainer router1Node = NodeContainer (nodes.Get (2));
  NodeContainer router2Node = NodeContainer (nodes.Get (3));
  NodeContainer extraNode1 = NodeContainer (nodes.Get(4));
  NodeContainer extraNode2 = NodeContainer (nodes.Get(5));

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer srcRouter1Devs = p2p.Install (sourceNode, router1Node);
  NetDeviceContainer srcRouter2Devs = p2p.Install (sourceNode, router2Node);
  NetDeviceContainer router1DestDevs = p2p.Install (router1Node, destNode);
  NetDeviceContainer router2DestDevs = p2p.Install (router2Node, destNode);
  NetDeviceContainer router1Extra1Devs = p2p.Install (router1Node, extraNode1);
  NetDeviceContainer router2Extra2Devs = p2p.Install (router2Node, extraNode2);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer srcRouter1Ifaces = address.Assign (srcRouter1Devs);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer srcRouter2Ifaces = address.Assign (srcRouter2Devs);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer router1DestIfaces = address.Assign (router1DestDevs);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer router2DestIfaces = address.Assign (router2DestDevs);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer router1Extra1Ifaces = address.Assign (router1Extra1Devs);

  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer router2Extra2Ifaces = address.Assign (router2Extra2Devs);

  // Configure routing
  Ipv4StaticRoutingHelper staticRouting;

  // Route from Router1 to Dest
  Ptr<Ipv4StaticRouting> router1Routing = staticRouting.GetStaticRouting (router1Node.Get (0)->GetObject<Ipv4> ());
  router1Routing->AddHostRouteTo (Ipv4Address ("10.1.3.2"), 1, router1DestIfaces.GetInterface (1)->GetIfIndex());

  // Route from Router2 to Dest
  Ptr<Ipv4StaticRouting> router2Routing = staticRouting.GetStaticRouting (router2Node.Get (0)->GetObject<Ipv4> ());
  router2Routing->AddHostRouteTo (Ipv4Address ("10.1.4.2"), 1, router2DestIfaces.GetInterface (1)->GetIfIndex());

  // Route from Source to Dest via Router1 (lower metric)
  Ptr<Ipv4StaticRouting> sourceRouting = staticRouting.GetStaticRouting (sourceNode.Get (0)->GetObject<Ipv4> ());
  sourceRouting->AddHostRouteTo (Ipv4Address ("10.1.3.2"), 1, srcRouter1Ifaces.GetInterface (0)->GetIfIndex());

  // Route from Source to Dest via Router2 (higher metric)
  sourceRouting->AddHostRouteTo (Ipv4Address ("10.1.4.2"), 2, srcRouter2Ifaces.GetInterface (0)->GetIfIndex());

  // Enable global routing (needed for simulation, but static routes are in place)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create UDP application
  UdpClientServerHelper udpClient (9, Ipv4Address ("10.1.3.2"), 5000); // send to Dest via Router1
  udpClient.SetAttribute ("MaxPackets", UintegerValue (10));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = udpClient.Install (sourceNode);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (12.0));

  UdpServerHelper udpServer (5000);
  ApplicationContainer serverApps = udpServer.Install (destNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (13.0));

  // Set up packet sinks and connect receive callbacks
  Ptr<Node> sourceNodePtr = sourceNode.Get (0);
  Ptr<Node> destNodePtr = destNode.Get (0);

  Ptr<Application> serverApp = serverApps.Get (0);
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApp);
  server->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallbackDestination));

  Ptr<Application> clientApp = clientApps.Get (0);
  Ptr<UdpClient> client = DynamicCast<UdpClient> (clientApp);
  client->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallbackSource));

  // Run simulation
  Simulator::Stop (Seconds (14.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}