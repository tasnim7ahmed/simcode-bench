// split-horizon-dv.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-route.h"
#include "ns3/distance-vector-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SplitHorizonDVExample");

int
main (int argc, char *argv[])
{
  // Enable logging, if desired
  // LogComponentEnable ("SplitHorizonDVExample", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create three nodes: A, B, C
  NodeContainer nodes;
  nodes.Create (3);

  // Point-to-point links: A <-> B <-> C
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect A-B
  NetDeviceContainer devAB = p2p.Install (nodes.Get (0), nodes.Get (1));
  // Connect B-C
  NetDeviceContainer devBC = p2p.Install (nodes.Get (1), nodes.Get (2));

  // Install Internet stack with Distance Vector routing (with split horizon)
  InternetStackHelper stack;

  // Enable Distance Vector with Split Horizon on all nodes
  Ipv4DistanceVectorRoutingHelper dvRouting;
  dvRouting.Set ("SplitHorizon", BooleanValue (true));
  // Use only DV as routing (no static or global)
  Ipv4ListRoutingHelper listRH;
  listRH.Add (dvRouting, 0);

  stack.SetRoutingHelper (listRH);
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  // Network between A-B
  address.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifAB = address.Assign (devAB);

  // Network between B-C
  address.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifBC = address.Assign (devBC);

  // Set up a UDP echo server on node C
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApp = echoServer.Install (nodes.Get (2));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (20.0));

  // Set up a UDP echo client on node A
  UdpEchoClientHelper echoClient (ifBC.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (2));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApp = echoClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (5.0));
  clientApp.Stop (Seconds (20.0));

  // Enable routing tables printing
  Ipv4DistanceVectorRoutingHelper::PrintRoutingTableAllAt (Seconds (4.0), Create<OutputStreamWrapper> (&std::cout));

  Simulator::Stop (Seconds (21.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}