#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-header.h"
#include "ns3/icmpv6-header.h"
#include "ns3/packet.h"
#include "ns3/ipv6-raw-socket-factory.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LooseSourceRouteIpv6Example");

class LooseSourceRoutingApp : public Application
{
public:
  LooseSourceRoutingApp ();
  virtual ~LooseSourceRoutingApp ();
  void Setup (Ptr<Socket> socket,
              Ipv6Address dst,
              std::vector<Ipv6Address> looseRoute,
              uint32_t packetSize,
              uint32_t nPackets,
              Time interval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Ipv6Address     m_dst;
  std::vector<Ipv6Address> m_looseRoute;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  uint32_t        m_sent;
  Time            m_interval;
  EventId         m_sendEvent;
};

LooseSourceRoutingApp::LooseSourceRoutingApp ()
  : m_socket (0),
    m_dst (),
    m_looseRoute (),
    m_packetSize (64),
    m_nPackets (1),
    m_sent (0),
    m_interval (Seconds (1.0))
{
}

LooseSourceRoutingApp::~LooseSourceRoutingApp ()
{
  m_socket = 0;
}

void
LooseSourceRoutingApp::Setup (Ptr<Socket> socket,
           Ipv6Address dst,
           std::vector<Ipv6Address> looseRoute,
           uint32_t packetSize,
           uint32_t nPackets,
           Time interval)
{
  m_socket = socket;
  m_dst = dst;
  m_looseRoute = looseRoute;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
}

void
LooseSourceRoutingApp::StartApplication (void)
{
  m_sent = 0;
  m_socket->Bind ();
  m_socket->Connect (Inet6SocketAddress (m_dst, 0));
  SendPacket ();
}

void
LooseSourceRoutingApp::StopApplication (void)
{
  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
LooseSourceRoutingApp::SendPacket (void)
{
  // Construct ICMPv6 Echo Request
  Ptr<Packet> p = Create<Packet> (m_packetSize - 8); // 8 bytes for header

  Icmpv6Echo echo;
  echo.SetSequenceNumber (m_sent);
  echo.SetIdentifier (0x1234);

  Icmpv6Header icmpv6;
  icmpv6.SetType (Icmpv6Header::ICMPV6_ECHO_REQUEST);
  icmpv6.SetCode (0);
  icmpv6.SetChecksum (0);

  p->AddHeader (echo);
  p->AddHeader (icmpv6);

  // Construct IPv6 header with Routing Header (Type 0 deprecated, but for sim)
  Ipv6Header ip6h;
  ip6h.SetSourceAddress (m_socket->GetNode ()->GetObject<Ipv6> ()->GetAddress (1, 0).GetAddress ());
  ip6h.SetDestinationAddress (m_dst);
  ip6h.SetNextHeader (Ipv6Header::IPV6_EXT_ROUTING);

  // Prepare Routing Header
  uint8_t segmentsLeft = m_looseRoute.size ();
  uint8_t routingType = 0;
  uint8_t routingHdrLength = 2*m_looseRoute.size ();
  uint32_t routingHdrSize = 8 + 16 * m_looseRoute.size ();
  Buffer buf (routingHdrSize);
  Buffer::Iterator i = buf.Begin ();
  i.WriteU8 (0); // Next header; will be ICMPv6
  i.WriteU8 (routingHdrLength);
  i.WriteU8 (routingType); // Type 0
  i.WriteU8 (segmentsLeft);
  i.WriteU32 (0); // Reserved

  for (std::vector<Ipv6Address>::const_iterator it = m_looseRoute.begin (); it != m_looseRoute.end (); ++it)
    {
      Ipv6Address addr = *it;
      for (uint32_t idx = 0; idx < 16; ++idx)
        {
          i.WriteU8 (addr.GetBytes ()[idx]);
        }
    }
  // Attach IPv6 header & Routing Header manually
  ip6h.SetPayloadLength (p->GetSize () + routingHdrSize);
  p->AddAtStart (buf);

  // Set next header for routing header
  ip6h.SetNextHeader (Ipv6Header::ICMPV6);

  p->AddHeader (ip6h);

  m_socket->Send (p);

  ++m_sent;
  if (m_sent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
LooseSourceRoutingApp::ScheduleTx (void)
{
  m_sendEvent = Simulator::Schedule (m_interval, &LooseSourceRoutingApp::SendPacket, this);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("LooseSourceRouteIpv6Example", LOG_LEVEL_INFO);

  // Create nodes: h0, h1, r0, r1, r2, r3
  NodeContainer hosts, routers;
  hosts.Create (2);
  routers.Create (4);
  Ptr<Node> h0 = hosts.Get (0);
  Ptr<Node> h1 = hosts.Get (1);
  Ptr<Node> r0 = routers.Get (0);
  Ptr<Node> r1 = routers.Get (1);
  Ptr<Node> r2 = routers.Get (2);
  Ptr<Node> r3 = routers.Get (3);

  // Point-to-point links and node containers for each link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // h0--r0, r0--r1, r1--r2, r2--r3, r3--h1
  NodeContainer h0r0 (h0, r0);
  NodeContainer r0r1 (r0, r1);
  NodeContainer r1r2 (r1, r2);
  NodeContainer r2r3 (r2, r3);
  NodeContainer r3h1 (r3, h1);

  // Install devices
  NetDeviceContainer d_h0r0 = p2p.Install (h0r0);
  NetDeviceContainer d_r0r1 = p2p.Install (r0r1);
  NetDeviceContainer d_r1r2 = p2p.Install (r1r2);
  NetDeviceContainer d_r2r3 = p2p.Install (r2r3);
  NetDeviceContainer d_r3h1 = p2p.Install (r3h1);

  // Install Internet stack with IPv6 on hosts and routers
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (hosts);
  internetv6.Install (routers);

  // Assign IPv6 addresses to the links
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_h0r0 = ipv6.Assign (d_h0r0);
  ipv6.NewNetwork ();
  Ipv6InterfaceContainer i_r0r1 = ipv6.Assign (d_r0r1);
  ipv6.NewNetwork ();
  Ipv6InterfaceContainer i_r1r2 = ipv6.Assign (d_r1r2);
  ipv6.NewNetwork ();
  Ipv6InterfaceContainer i_r2r3 = ipv6.Assign (d_r2r3);
  ipv6.NewNetwork ();
  Ipv6InterfaceContainer i_r3h1 = ipv6.Assign (d_r3h1);

  i_h0r0.SetForwarding (1, true); // r0
  i_r0r1.SetForwarding (0, true); // r0
  i_r0r1.SetForwarding (1, true); // r1
  i_r1r2.SetForwarding (0, true); // r1
  i_r1r2.SetForwarding (1, true); // r2
  i_r2r3.SetForwarding (0, true); // r2
  i_r2r3.SetForwarding (1, true); // r3
  i_r3h1.SetForwarding (0, true); // r3

  // Assign addresses, set as default routes where needed
  i_h0r0.SetDefaultRouteInAllNodes (0);
  i_r3h1.SetDefaultRouteInAllNodes (1);

  // Configure static routing
  Ipv6StaticRoutingHelper staticRoutingHelper;

  // Host 0: default route to r0
  Ptr<Ipv6StaticRouting> h0StaticRouting =
    staticRoutingHelper.GetStaticRouting (h0->GetObject<Ipv6> ());
  h0StaticRouting->SetDefaultRoute (i_h0r0.GetAddress (1, 1), 1);

  // Host 1: default route to r3
  Ptr<Ipv6StaticRouting> h1StaticRouting =
    staticRoutingHelper.GetStaticRouting (h1->GetObject<Ipv6> ());
  h1StaticRouting->SetDefaultRoute (i_r3h1.GetAddress (0, 1), 1);

  // Routers: add routes accordingly
  Ptr<Ipv6StaticRouting> r0StaticRouting =
    staticRoutingHelper.GetStaticRouting (r0->GetObject<Ipv6> ());
  r0StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:4::"), Ipv6Prefix (64),
                                      i_r0r1.GetAddress (1, 1), 2);

  Ptr<Ipv6StaticRouting> r1StaticRouting =
    staticRoutingHelper.GetStaticRouting (r1->GetObject<Ipv6> ());
  r1StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:1::"), Ipv6Prefix (64),
                                      i_r0r1.GetAddress (0, 1), 1);
  r1StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:4::"), Ipv6Prefix (64),
                                      i_r1r2.GetAddress (1, 1), 2);

  Ptr<Ipv6StaticRouting> r2StaticRouting =
    staticRoutingHelper.GetStaticRouting (r2->GetObject<Ipv6> ());
  r2StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:2::"), Ipv6Prefix (64),
                                      i_r1r2.GetAddress (0, 1), 1);
  r2StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:5::"), Ipv6Prefix (64),
                                      i_r2r3.GetAddress (1, 1), 2);

  Ptr<Ipv6StaticRouting> r3StaticRouting =
    staticRoutingHelper.GetStaticRouting (r3->GetObject<Ipv6> ());
  r3StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:3::"), Ipv6Prefix (64),
                                      i_r2r3.GetAddress (0, 1), 1);
  r3StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:5::"), Ipv6Prefix (64),
                                      i_r3h1.GetAddress (1, 1), 2);

  // Host 1: Echo server
  uint16_t echoPort = 9;
  Ipv6Address h1Addr = i_r3h1.GetAddress (1, 1);
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (h1);
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Host 0: custom loose source routing echo application
  Ptr<Socket> srcSocket = Socket::CreateSocket (h0, TypeId::LookupByName ("ns3::Ipv6RawSocketFactory"));

  std::vector<Ipv6Address> lsrRoute;
  lsrRoute.push_back (i_r0r1.GetAddress (1, 1)); // r1
  lsrRoute.push_back (i_r1r2.GetAddress (1, 1)); // r2
  lsrRoute.push_back (i_r2r3.GetAddress (1, 1)); // r3
  lsrRoute.push_back (i_r3h1.GetAddress (1, 1)); // h1

  Ptr<LooseSourceRoutingApp> srcApp = CreateObject<LooseSourceRoutingApp> ();
  srcApp->Setup (srcSocket, h1Addr, lsrRoute, 64, 4, Seconds (1.0));
  h0->AddApplication (srcApp);
  srcApp->SetStartTime (Seconds (1.0));
  srcApp->SetStopTime (Seconds (9.0));

  // Packet tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("loose-routing-ipv6.tr"));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}