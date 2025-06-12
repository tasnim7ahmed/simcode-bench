#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeUdpEchoExample");

void
TxTrace (Ptr<const Packet> packet)
{
  NS_LOG_INFO ("Packet transmitted: " << *packet);
}

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO ("Packet received: " << *packet);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("ThreeNodeUdpEchoExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  // Point-to-Point links between 0-1 and 1-2
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = p2p.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices12 = p2p.Install (nodes.Get(1), nodes.Get(2));

  // Install the Internet stack on all nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses to both point-to-point links
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  // Set up routing on node 1 (enable forwarding)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo server on node 2 (ID 2)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo client on node 0 (ID 0) -> node 2 (ID 2)
  UdpEchoClientHelper echoClient (interfaces12.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Log packet transmissions on device 0 (node 0 to node 1)
  devices01.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxTrace));
  // Log packet receptions on device 1 (node 1 from node 0)
  devices01.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));
  // Log transmissions from node 1 to node 2
  devices12.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxTrace));
  // Log receptions at node 2
  devices12.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}