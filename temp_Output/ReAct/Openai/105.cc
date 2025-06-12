#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationExample");

void
RxTrace (Ptr<const Packet> pkt, const Address &address)
{
  static std::ofstream rxtrace ("fragmentation-ipv6.tr", std::ios_base::app);
  rxtrace << Simulator::Now ().GetSeconds ()
          << " RX: Packet UID=" << pkt->GetUid ()
          << " Size=" << pkt->GetSize () << std::endl;
}

void
QueueEnqueueTrace (Ptr<const Packet> packet)
{
  static std::ofstream qtrace ("fragmentation-ipv6.tr", std::ios_base::app);
  qtrace << Simulator::Now ().GetSeconds ()
         << " QUEUE_ENQUEUE: Packet UID=" << packet->GetUid ()
         << " Size=" << packet->GetSize () << std::endl;
}

void
QueueDequeueTrace (Ptr<const Packet> packet)
{
  static std::ofstream qtrace ("fragmentation-ipv6.tr", std::ios_base::app);
  qtrace << Simulator::Now ().GetSeconds ()
         << " QUEUE_DEQUEUE: Packet UID=" << packet->GetUid ()
         << " Size=" << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6StaticRouting", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // n0=0, r=1, n1=2

  // Set up CSMA links: n0<---->r<---->n1
  NodeContainer n0r (nodes.Get (0), nodes.Get (1));
  NodeContainer r1n (nodes.Get (1), nodes.Get (2));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer d0r = csma.Install (n0r);
  NetDeviceContainer dr1 = csma.Install (r1n);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6a;
  ipv6a.SetBase (Ipv6Address ("2001:0:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0r = ipv6a.Assign (d0r);

  Ipv6AddressHelper ipv6b;
  ipv6b.SetBase (Ipv6Address ("2001:0:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ir1 = ipv6b.Assign (dr1);

  i0r.SetForwarding (1, true); // r interface on n0r
  i0r.SetDefaultRouteInAllNodes (0); // n0's default route

  ir1.SetForwarding (0, true); // r interface on n1r
  ir1.SetDefaultRouteInAllNodes (1); // n1's default route

  // Static Routing
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting0 = ipv6RoutingHelper.GetStaticRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting0->SetDefaultRoute (i0r.GetAddress (1, 1), 1);

  Ptr<Ipv6StaticRouting> staticRouting2 = ipv6RoutingHelper.GetStaticRouting (nodes.Get (2)->GetObject<Ipv6> ());
  staticRouting2->SetDefaultRoute (ir1.GetAddress (0, 1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingR = ipv6RoutingHelper.GetStaticRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRoutingR->AddNetworkRouteTo (Ipv6Address ("2001:0:0:1::"), Ipv6Prefix (64), 1);
  staticRoutingR->AddNetworkRouteTo (Ipv6Address ("2001:0:0:2::"), Ipv6Prefix (64), 2);

  // Set small MTU to force fragmentation (e.g. 400 bytes)
  for (uint32_t i = 0; i < d0r.GetN (); ++i)
    {
      d0r.Get (i)->SetMtu (400);
    }
  for (uint32_t i = 0; i < dr1.GetN (); ++i)
    {
      dr1.Get (i)->SetMtu (400);
    }

  // UDP Echo Server (n1)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client (n0) -> n1
  UdpEchoClientHelper echoClient (ir1.GetAddress (1, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1000)); // Large enough to require fragmentation

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Trace QUEUE events for both links
  Ptr<Queue<Packet> > queue0 = d0r.Get (0)->GetObject<Queue<Packet> > ();
  if (queue0)
    {
      queue0->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      queue0->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
    }
  Ptr<Queue<Packet> > queue1 = dr1.Get (0)->GetObject<Queue<Packet> > ();
  if (queue1)
    {
      queue1->TraceConnectWithoutContext ("Enqueue", MakeCallback (&QueueEnqueueTrace));
      queue1->TraceConnectWithoutContext ("Dequeue", MakeCallback (&QueueDequeueTrace));
    }

  // Trace packet reception on n1
  Ptr<Node> n1 = nodes.Get (2);
  Ptr<Ipv6L3Protocol> ipv6l3 = n1->GetObject<Ipv6L3Protocol> ();
  if (ipv6l3)
    {
      ipv6l3->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));
    }

  // Enable pcap tracing
  csma.EnablePcapAll ("fragmentation-ipv6", true);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}