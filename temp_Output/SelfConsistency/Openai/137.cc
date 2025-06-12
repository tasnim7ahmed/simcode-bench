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
  NS_LOG_INFO ("Packet transmitted at " << Simulator::Now ().GetSeconds () << "s");
}

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO ("Packet received at " << Simulator::Now ().GetSeconds () << "s");
}

int 
main (int argc, char *argv[])
{
  // Enable logging for echo applications
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("ThreeNodeUdpEchoExample", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Point-to-point helper
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect n0 <-> n1
  NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d0d1 = p2p.Install (n0n1);

  // Connect n1 <-> n2
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d1d2 = p2p.Install (n1n2);

  // Install the Internet stack on all nodes
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  // Enable routing on node 1 (the router)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up UDP Echo Server on node 2 (index 2)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Set up UDP Echo Client on node 0, sending to node 2's address on 10.1.2.2
  UdpEchoClientHelper echoClient (i1i2.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Trace packet transmissions and receptions
  // Trace on node 0 (client) device for transmission
  d0d1.Get (0)->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&TxTrace));
  // Trace on node 2 (server) device for reception
  d1d2.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}