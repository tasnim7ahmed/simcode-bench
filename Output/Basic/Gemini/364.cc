#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/icmpv6-l4-protocol.h"
#include "ns3/ipv6-endpoint.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6Ping");

static void
PingRtt (std::string context, Time rtt)
{
  NS_LOG_UNCOND (context << " RTT " << rtt);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::Enable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
  LogComponent::Enable ("Ipv6Ping", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  // Set up static routing (required for ping to work)
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetmask (Ipv6Address ("2001:1::2"), Ipv6Prefix (64), 1, interfaces.GetAddress (1, 1));

  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetmask (Ipv6Address ("2001:1::1"), Ipv6Prefix (64), 1, interfaces.GetAddress (0, 1));

  Ptr<Node> client = nodes.Get (0);
  Ptr<Icmpv6L4Protocol> icmpv6 = client->GetObject<Ipv6> ()->GetProtocol (58)->GetObject<Icmpv6L4Protocol> ();
  icmpv6->TraceConnectWithoutContext ("RttEstimates", MakeCallback (&PingRtt));

  TypeId tid = TypeId::LookupByName ("ns3::Icmpv6L4Protocol");
  Ptr<Socket> socket = Socket::CreateSocket (client, tid);

  socket->SetAttribute ("Icmpv6Type", UintegerValue (128));
  socket->SetAttribute ("Icmpv6Code", UintegerValue (0));

  Ipv6Endpoint dest = Ipv6Endpoint (interfaces.GetAddress (1, 1), 0);
  socket->Connect (dest);

  Time interPingInterval = Seconds (1);
  Time startTime = Seconds (1);
  uint32_t packetSize = 1024;
  uint32_t maxPackets = 10;

  Simulator::Schedule (startTime, &Icmpv6L4Protocol::Ping, icmpv6, socket, dest.GetIpv6 (), packetSize, maxPackets, interPingInterval);

  Simulator::Stop (Seconds (10));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}