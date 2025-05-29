#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/radvd-helper.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdTwoPrefixExample");

void
ReceivePacket (Ptr<const Packet> packet, Ptr<Ipv6> ipv6, uint32_t interface)
{
  static std::ofstream out ("radvd-two-prefix.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds ()
      << " RX PACKET interface=" << interface
      << " size=" << packet->GetSize ()
      << std::endl;
}

void
QueueTrace (std::string context, Ptr<const Packet> p)
{
  static std::ofstream out ("radvd-two-prefix.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds ()
      << " QUEUE EVENT " << context << " size=" << p->GetSize ()
      << std::endl;
}

void
PrintIpv6Addresses (NodeContainer& nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
      NS_LOG_INFO ("Node " << node->GetId ());
      for (uint32_t j = 0; j < ipv6->GetNInterfaces (); ++j)
        {
          for (uint32_t a = 0; a < ipv6->GetNAddresses (j); ++a)
            {
              Ipv6InterfaceAddress addr = ipv6->GetAddress (j, a);
              if (!addr.GetAddress ().IsLinkLocal ())
                {
                  NS_LOG_INFO ("  Interface=" << j
                               << " Addr=" << addr.GetAddress ()
                               << "/" << addr.GetPrefixLength ());
                }
            }
        }
    }
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("RadvdTwoPrefixExample", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2); // n0,n1
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Node> r = CreateObject<Node> (); // Router

  NodeContainer csma0Nodes;
  csma0Nodes.Add (n0);
  csma0Nodes.Add (r);

  NodeContainer csma1Nodes;
  csma1Nodes.Add (n1);
  csma1Nodes.Add (r);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer d0 = csma.Install (csma0Nodes); // [n0, R]
  NetDeviceContainer d1 = csma.Install (csma1Nodes); // [n1, R]

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (n0);
  internetv6.Install (n1);
  internetv6.Install (r);

  // Assign IPv6 Addresses
  Ipv6AddressHelper ipv6_0;
  ipv6_0.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0 = ipv6_0.Assign (d0);

  // Assign second prefix to R's CSMA0 interface
  Ptr<Ipv6> rIpv6 = r->GetObject<Ipv6> ();
  int rIf0 = rIpv6->GetInterfaceForDevice (d0.Get (1));
  rIpv6->AddAddress (rIf0, Ipv6InterfaceAddress (Ipv6Address ("2001:ABCD::1"), Ipv6Prefix (64)));
  rIpv6->SetUp (rIf0);

  Ipv6AddressHelper ipv6_1;
  ipv6_1.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1 = ipv6_1.Assign (d1);

  // Set all interfaces up (including R's second address interface)
  for (uint32_t i = 0; i < i0.GetN (); ++i)
    i0.SetForwarding (i, true);
  for (uint32_t i = 0; i < i1.GetN (); ++i)
    i1.SetForwarding (i, true);

  // Enable global routing for n0 and n1 (not for R, as we'll use radvd)
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> n0Routing = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  n0Routing->SetDefaultRoute (i0.GetAddress (1, 1), 1);
  Ptr<Ipv6StaticRouting> n1Routing = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  n1Routing->SetDefaultRoute (i1.GetAddress (1, 1), 1);

  // Configure RADVD on router
  RadvdHelper radvdHelper;

  // Two prefixes for csma0 subnet
  RadvdInterface radvdIf0 (d0.Get (1));
  radvdIf0.AddPrefix (RadvdPrefix ("2001:1::", 64));
  radvdIf0.AddPrefix (RadvdPrefix ("2001:ABCD::", 64));
  radvdIf0.SetHomeAgentFlag (false);

  // One prefix for csma1 subnet
  RadvdInterface radvdIf1 (d1.Get (1));
  radvdIf1.AddPrefix (RadvdPrefix ("2001:2::", 64));
  radvdIf1.SetHomeAgentFlag (false);

  std::vector<RadvdInterface> radvdInterfaces;
  radvdInterfaces.push_back (radvdIf0);
  radvdInterfaces.push_back (radvdIf1);
  ApplicationContainer radvds = radvdHelper.Install (r, radvdInterfaces);

  radvds.Start (Seconds (0.5));
  radvds.Stop (Seconds (20.0));

  // Tracing
  Ptr<Queue<Packet> > q0 = d0.Get (0)->GetObject<Queue<Packet> >();
  if (q0)
    {
      q0->TraceConnect ("Enqueue", "n0-csma0", MakeCallback (&QueueTrace));
      q0->TraceConnect ("Dequeue", "n0-csma0", MakeCallback (&QueueTrace));
    }
  Ptr<Queue<Packet> > q1 = d1.Get (0)->GetObject<Queue<Packet> >();
  if (q1)
    {
      q1->TraceConnect ("Enqueue", "n1-csma1", MakeCallback (&QueueTrace));
      q1->TraceConnect ("Dequeue", "n1-csma1", MakeCallback (&QueueTrace));
    }

  n1->GetObject<Ipv6> ()->TraceConnectWithoutContext ("Receive", MakeCallback (&ReceivePacket));

  // Wait for autoconfig to complete before pinging; n0 should have two addresses
  Simulator::Schedule (Seconds (3.0), &PrintIpv6Addresses, nodes);

  // Start a ping6 app: n0 -> one of n1's addresses
  V6PingHelper ping6 (i1.GetAddress (0, 1));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer apps = ping6.Install (n0);
  apps.Start (Seconds (4.0));
  apps.Stop (Seconds (19.0));

  Simulator::Stop (Seconds (20.0));

  AsciiTraceHelper ath;
  csma.EnableAsciiAll (ath.CreateFileStream ("radvd-two-prefix.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}