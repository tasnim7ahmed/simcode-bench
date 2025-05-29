#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/command-line.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6Options");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  Ipv6Address ipv6From = InetSocketAddress::ConvertFrom(from).GetIpv6 ();
  uint32_t recvTClass;
  uint8_t recvHopLimit;
  socket->GetSockOpt (IPV6_TCLASS, recvTClass);
  socket->GetSockOpt (IPV6_HOPLIMIT, recvHopLimit);

  NS_LOG_UNCOND ("Received one packet from " << ipv6From << " at time " << Simulator::Now ().GetSeconds ()
                                            << "s TCLASS " << (uint32_t)recvTClass << " HOPLIMIT " << (uint32_t)recvHopLimit);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0;
  uint32_t tclassValue = 0;
  uint8_t hopLimitValue = 1;
  cmd.AddValue ("packetSize", "The size of the packets", packetSize);
  cmd.AddValue ("numPackets", "The number of packets", numPackets);
  cmd.AddValue ("interval", "The time between packets", interval);
  cmd.AddValue ("tclass", "The TCLASS value", tclassValue);
  cmd.AddValue ("hoplimit", "The HOPLIMIT value", hopLimitValue);
  cmd.Parse (argc, argv);

  Time interPacketInterval = Seconds (interval);

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  // Set up static routes
  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (0)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (interfaces.GetAddress (1, 1), 0);
  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (1)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (interfaces.GetAddress (0, 1), 0);

  uint16_t port = 9;
  UdpClientServerHelper client (interfaces.GetAddress (1, 1), port);
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));

  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (1), TypeId::LookupByName ("ns3::UdpSocketFactory6"));
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), port);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
  recvSink->SetAttribute ("IpTtl", UintegerValue (0));
  recvSink->SetSockOpt (RECV_TCLASS, 1);
  recvSink->SetSockOpt (RECV_HOPLIMIT, 1);

  clientApps.Start (Seconds (1.0));

  Ptr<Socket> source = clientApps.Get (0)->GetObject<Application> ()->GetSocket ();
  source->SetSockOpt (IPV6_TCLASS, tclassValue);
  source->SetSockOpt (IPV6_HOPLIMIT, hopLimitValue);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}