#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/ipv6-endpoint.h"
#include "ns3/uinteger.h"

using namespace ns3;

static void
RxCallback (Ptr<Socket> socket)
{
  Address remoteAddress;
  auto packet = socket->RecvFrom (remoteAddress);
  Ipv6Address sourceAddress = Ipv6Address::ConvertFrom (remoteAddress);
  uint8_t hopLimit;
  socket->GetSockOptHopLimit (hopLimit);

  int32_t trafficClass;
  socket->GetSockOptTrafficClass (trafficClass);

  std::cout << Simulator::Now ().AsTime (Time::S) << "s "
            << "RxCallback: Received one packet from " << sourceAddress << " HopLimit " << (uint32_t) hopLimit << " TrafficClass " << trafficClass
            << std::endl;
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0;
  int32_t tclass = 0;
  uint8_t hoplimit = 64;

  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets", numPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.AddValue ("tclass", "Traffic Class value", tclass);
  cmd.AddValue ("hoplimit", "Hop Limit value", hoplimit);

  cmd.Parse (argc, argv);

  Time interPacketInterval = Seconds (interval);

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetStaticRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetmask (Ipv6Address ("2001:db8::2"), Ipv6Prefix (128), Ipv6Address ("2001:db8::2"), 1);
  staticRouting = Ipv6RoutingHelper::GetStaticRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetmask (Ipv6Address ("2001:db8::1"), Ipv6Prefix (128), Ipv6Address ("2001:db8::1"), 1);

  uint16_t port = 9;

  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), TypeId::LookupByName ("ns3::UdpSocketIpv6"));
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&RxCallback));
  recvSocket->SetAttribute ("IpTtl", UintegerValue (255));
  recvSocket->SetAttribute("RecvTclass", BooleanValue(true));
  recvSocket->SetAttribute("RecvHopLimit", BooleanValue(true));

  Ptr<Socket> sourceSocket = Socket::CreateSocket (nodes.Get (0), TypeId::LookupByName ("ns3::UdpSocketIpv6"));
  sourceSocket->SetAttribute ("IpTtl", UintegerValue (255));

  sourceSocket->SetSockOptHopLimit (hoplimit);
  sourceSocket->SetSockOptTrafficClass (tclass);

  Inet6SocketAddress remote = Inet6SocketAddress (interfaces.GetAddress (1, 1), port);
  sourceSocket->Connect (remote);

  ApplicationContainer sourceApp;

  for (uint32_t i = 0; i < numPackets; ++i)
    {
      Simulator::Schedule (interPacketInterval * i, [=]() {
        Ptr<Packet> packet = Create<Packet> (packetSize);
        sourceSocket->Send (packet);
      });
    }

  Simulator::Stop (interPacketInterval * numPackets);
  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}