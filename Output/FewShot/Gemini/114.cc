#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/propagation-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/icmpv6-header.h"
#include "ns3/ipv6-endpoint.h"
#include "ns3/socket.h"
#include "ns3/callback.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnPing6");

static void
ReceivePing6(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);
  Ipv6Address sender = Inet6SocketAddress::ConvertFrom(from).GetIpv6();
  Icmpv6Header ping;
  packet->RemoveHeader(ping);
  NS_LOG_INFO("Received an ICMPv6 echo reply from " << sender);
}

static void
SendPing6(Ptr<Socket> socket, Ipv6Address dest)
{
  Icmpv6Header ping;
  ping.SetType(Icmpv6Header::ECHO);
  ping.SetCode(0);
  Ptr<Packet> packet = Create<Packet>();
  packet->AddHeader(ping);
  socket->SendTo(packet, 0, Inet6SocketAddress(dest, 60000));
}

int
main(int argc, char* argv[])
{
  LogComponentEnable("WsnPing6", LOG_LEVEL_INFO);
  LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);

  bool verbose = false;
  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.Parse(argc, argv);

  if (verbose)
    {
      LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create(2);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(nodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install(lrWpanDevices);

  InternetStackHelper internetv6;
  internetv6.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(sixLowPanDevices);
  interfaces.SetForwarding(0, true);
  interfaces.SetForwarding(1, true);
  interfaces.SetDefaultRouteInAllNodes();

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  ipv6RoutingHelper.PopulateRoutingTables();

  Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), TypeId::LookupByName("ns3::Ipv6RawSocketFactory"));
  recvSocket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), 60000));
  recvSocket->SetRecvCallback(MakeCallback(&ReceivePing6));

  Simulator::Schedule(Seconds(1.0), &SendPing6, Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::Ipv6RawSocketFactory")), interfaces.GetAddress(1,1));
  Simulator::Schedule(Seconds(2.0), &SendPing6, Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::Ipv6RawSocketFactory")), interfaces.GetAddress(1,1));
  Simulator::Schedule(Seconds(3.0), &SendPing6, Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::Ipv6RawSocketFactory")), interfaces.GetAddress(1,1));

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("wsn-ping6.tr");
  CsmaHelper csma;
  csma.EnableAsciiAll(stream);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}