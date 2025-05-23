#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStarTopology");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RipStarTopology", LOG_LEVEL_INFO);
  LogComponentEnable ("Rip", LOG_LEVEL_DEBUG);
  LogComponentEnable ("Ipv4RoutingTable", LOG_LEVEL_DEBUG);
  LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);

  NodeContainer centralNode;
  centralNode.Create (1);
  NodeContainer edgeNodes;
  edgeNodes.Create (4);

  NodeContainer allNodes;
  allNodes.Add (centralNode);
  allNodes.Add (edgeNodes);

  RipHelper ripRouting;
  Ipv4ListRoutingHelper listRouting;
  listRouting.Add (ripRouting, 10);
  Ipv4StaticRoutingHelper staticRouting;
  listRouting.Add (staticRouting, 0);

  InternetStackHelper internet;
  internet.SetIpv4RoutingHelper (listRouting);
  internet.Install (allNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer central_E1_devices;
  NetDeviceContainer central_E2_devices;
  NetDeviceContainer central_E3_devices;
  NetDeviceContainer central_E4_devices;

  Ipv4AddressHelper ipv4;

  // Link 1: Central <-> E1 (Node 0 <-> Node 1)
  central_E1_devices = p2p.Install (centralNode.Get (0), edgeNodes.Get (0));
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer central_E1_interfaces = ipv4.Assign (central_E1_devices);

  // Link 2: Central <-> E2 (Node 0 <-> Node 2)
  central_E2_devices = p2p.Install (centralNode.Get (0), edgeNodes.Get (1));
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer central_E2_interfaces = ipv4.Assign (central_E2_devices);

  // Link 3: Central <-> E3 (Node 0 <-> Node 3)
  central_E3_devices = p2p.Install (centralNode.Get (0), edgeNodes.Get (2));
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer central_E3_interfaces = ipv4.Assign (central_E3_devices);

  // Link 4: Central <-> E4 (Node 0 <-> Node 4)
  central_E4_devices = p2p.Install (centralNode.Get (0), edgeNodes.Get (3));
  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer central_E4_interfaces = ipv4.Assign (central_E4_devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Enable RIP tracing for all nodes
  ripRouting.EnableTracing ("rip-star-topology", centralNode.Get(0), MilliSeconds (100));
  for (uint32_t i = 0; i < edgeNodes.GetN(); ++i)
  {
    ripRouting.EnableTracing ("rip-star-topology", edgeNodes.Get(i), MilliSeconds (100));
  }

  // Optional: Add some application traffic to verify connectivity
  uint16_t port = 9; // Discard port
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (edgeNodes.Get (3)); // E4
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (60.0));

  // E4's IP is its second interface (index 1), loopback is index 0
  Ipv4Address E4_IP_Address = edgeNodes.Get (3)->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
  UdpEchoClientHelper echoClient (E4_IP_Address, port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (edgeNodes.Get (0)); // E1
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (60.0));

  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}