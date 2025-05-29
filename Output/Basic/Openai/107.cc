#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/ipv6-route.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

void
PacketReceptionTrace (Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream out ("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
  out << "Rx: time=" << Simulator::Now ().GetSeconds () << " s, "
      << "size=" << packet->GetSize () << std::endl;
}

void
QueueEnqueueTrace (Ptr<const QueueDiscItem> item)
{
  static std::ofstream out ("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
  out << "Enqueue: time=" << Simulator::Now ().GetSeconds () << " s, "
      << "size=" << item->GetPacket ()->GetSize () << std::endl;
}

void
QueueDequeueTrace (Ptr<const QueueDiscItem> item)
{
  static std::ofstream out ("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
  out << "Dequeue: time=" << Simulator::Now ().GetSeconds () << " s, "
      << "size=" << item->GetPacket ()->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  // Nodes: n0 -- r -- n1
  NodeContainer n0r;
  n0r.Create (2);
  NodeContainer rn1;
  rn1.Add (n0r.Get (1));
  rn1.Create (1);

  Ptr<Node> n0 = n0r.Get (0);
  Ptr<Node> r  = n0r.Get (1);
  Ptr<Node> n1 = rn1.Get (1);

  // CSMA channels
  CsmaHelper csma0, csma1;
  csma0.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma0.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma0.SetDeviceAttribute ("Mtu", UintegerValue (5000));

  csma1.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma1.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma1.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  NetDeviceContainer d0 = csma0.Install (NodeContainer (n0, r));
  NetDeviceContainer d1 = csma1.Install (NodeContainer (r, n1));

  // Internet stack
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (NodeContainer (n0, r, n1));

  // IPv6 assignments
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_n0r = ipv6.Assign (d0);
  if_n0r.SetForwarding (0, true);
  if_n0r.SetDefaultRouteInAllNodes (0);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_rn1 = ipv6.Assign (d1);
  if_rn1.SetForwarding (0, true);  // router side of link

  // Ensure forwarding is enabled on r
  Ptr<Ipv6> ipv6r = r->GetObject<Ipv6> ();
  ipv6r->SetAttribute ("IpForward", BooleanValue (true));

  // UDP Echo Server on n1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on n0 to n1
  Ipv6Address dstAddr = if_rn1.GetAddress (1, 1); // n1's global address
  UdpEchoClientHelper echoClient (dstAddr, echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (n0);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable static routing (routes auto by global routing in IPv6, but add default for n0, n1)
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> n0StaticRouting = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  n0StaticRouting->SetDefaultRoute (if_n0r.GetAddress (1, 1), 1, 0);

  Ptr<Ipv6StaticRouting> n1StaticRouting = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  n1StaticRouting->SetDefaultRoute (if_rn1.GetAddress (0, 1), 1, 0);

  // Tracing
  // Queue traces for both links
  Ptr<NetDeviceQueueInterface> qif0 = d0.Get (0)->GetObject<NetDeviceQueueInterface> ();
  if (qif0 && qif0->GetTxQueue (0))
    {
      qif0->GetTxQueue (0)->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      qif0->GetTxQueue (0)->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
    }
  Ptr<NetDeviceQueueInterface> qif1 = d1.Get (0)->GetObject<NetDeviceQueueInterface> ();
  if (qif1 && qif1->GetTxQueue (0))
    {
      qif1->GetTxQueue (0)->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      qif1->GetTxQueue (0)->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
    }
  // Also receive tracing on server
  Ptr<Application> serverApp = serverApps.Get (0);
  Ptr<PacketSink> pktSink = DynamicCast<PacketSink> (serverApp);
  if (pktSink)
    {
      pktSink->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketReceptionTrace));
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}