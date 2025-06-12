#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
TxCallback (std::string context, Ptr<const Packet> packet)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s: Packet transmitted - " << packet->GetSize () << " bytes");
}

void
RxCallback (std::string context, Ptr<const Packet> packet, const Address & address)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s: Packet received - " << packet->GetSize () << " bytes");
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  // PointToPoint helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Link n0<->n1
  NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d0d1 = p2p.Install (n0n1);

  // Link n1<->n2
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d1d2 = p2p.Install (n1n2);

  // Install stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  // Set up routing on n1 (the router)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo Server on n2
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on n0, targeting n2
  UdpEchoClientHelper echoClient (i1i2.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing: log transmission (at device) and reception (at UDP socket of server)
  // Tx trace on node 0's NetDevice
  d0d1.Get (0)->TraceConnect ("PhyTxEnd", "", MakeCallback (&TxCallback));
  // Rx trace on server
  Config::Connect ("/NodeList/2/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback (&RxCallback));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}