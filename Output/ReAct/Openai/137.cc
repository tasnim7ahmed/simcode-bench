#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
TxTrace (Ptr<const Packet> packet)
{
  std::cout << Simulator::Now ().GetSeconds () << "s Client TX " << packet->GetSize () << " bytes" << std::endl;
}

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  std::cout << Simulator::Now ().GetSeconds () << "s Server RX " << packet->GetSize () << " bytes" << std::endl;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  // Create two point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address1;
  address1.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address1.Assign (devices1);

  Ipv4AddressHelper address2;
  address2.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address2.Assign (devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create UDP Echo Server on node 2 (index 2)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create UDP Echo Client on node 0 (index 0), targeting node 2
  UdpEchoClientHelper echoClient (interfaces2.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing transmission at client
  Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient> (clientApps.Get (0));
  client->TraceConnectWithoutContext ("Tx", MakeCallback (&TxTrace));

  // Tracing reception at server
  Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer> (serverApps.Get (0));
  server->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}