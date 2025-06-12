#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/udp-echo-client.h"
#include "ns3/udp-echo-server.h"
#include "ns3/packet-sink.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationTwoMtu");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (5);
  NodeContainer n0r = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer r_n1 = NodeContainer (nodes.Get (2), nodes.Get (3));

  // Create channels
  CsmaHelper csmaN0R;
  csmaN0R.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csmaN0R.SetChannelAttribute ("Delay", TimeValue (Seconds (0.002)));
  csmaN0R.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer d_n0r = csmaN0R.Install (n0r);

  CsmaHelper csmaR_N1;
  csmaR_N1.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csmaR_N1.SetChannelAttribute ("Delay", TimeValue (Seconds (0.002)));
  csmaR_N1.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer d_r_n1 = csmaR_N1.Install (r_n1);

  // Install IPv6
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n0r = ipv6.Assign (d_n0r);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_r_n1 = ipv6.Assign (d_r_n1);

  // Enable IPv6 forwarding on the router
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  ipv6RoutingHelper.PopulateRoutingTables ();

  // Setup routing from n0 to n1
  Ptr<Ipv6StaticRouting> n0Routing = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (1));
  n0Routing->AddHostRouteToNetmask (i_r_n1.GetAddress (1), Ipv6Prefix (64), i_n0r.GetAddress (1), 1);

  // Setup routing from n1 to n0
  Ptr<Ipv6StaticRouting> n1Routing = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (3));
  n1Routing->AddHostRouteToNetmask (i_n0r.GetAddress (0), Ipv6Prefix (64), i_r_n1.GetAddress (0), 1);

  // Create Applications
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3)); // Destination
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i_r_n1.GetAddress (1), 9); // N1 address as known to n0
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000)); //Large packet to trigger fragmentation

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1)); // Source
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing
  CsmaHelper::EnableAsciiAll ("fragmentation-ipv6-two-mtu.tr");

  // Run simulation
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}