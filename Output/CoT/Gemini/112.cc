#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-module.h"
#include "ns3/ping6.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdTwoPrefix");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("RadvdTwoPrefix", LOG_LEVEL_INFO);
      LogComponentEnable ("CsmaChannel", LOG_LEVEL_ALL);
      LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_ALL);
      LogComponentEnable ("Radvd", LOG_LEVEL_ALL);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_ALL);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  NodeContainer n0Router = NodeContainer (nodes.Get (0), router.Get (0));
  NodeContainer n1Router = NodeContainer (nodes.Get (1), router.Get (0));

  NS_LOG_INFO ("Create channels.");
  CsmaHelper csma0;
  csma0.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma0.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer nd0 = csma0.Install (n0Router);

  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer nd1 = csma1.Install (n1Router);

  NS_LOG_INFO ("Install IPv6 Internet stack on all nodes.");
  InternetStackHelper internetv6;
  internetv6.Install (nodes);
  internetv6.Install (router);

  NS_LOG_INFO ("Assign IPv6 Addresses.");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0 = ipv6.Assign (nd0);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1 = ipv6.Assign (nd1);

  // Router Address assignment
  Ptr<Node> routerNode = router.Get (0);
  Ptr<Ipv6> ipv6Router = routerNode->GetObject<Ipv6> ();
  Ipv6Address addr0 = Ipv6Address ("2001:1::1");
  Ipv6Address addr1 = Ipv6Address ("2001:2::1");
  ipv6Router->SetAddress (1, addr0);
  ipv6Router->SetAddress (2, addr1);
  ipv6Router->SetMetric (1, 1);
  ipv6Router->SetMetric (2, 1);
  ipv6Router->SetRouterAdvertisement (true, 1);
  ipv6Router->SetRouterAdvertisement (true, 2);
  i0.GetAddress (0,0);
  i1.GetAddress (0,0);

  // Set another address on n0's interface of the router
  Ipv6Address addr0_2 = Ipv6Address ("2001:ABCD::1");
  ipv6Router->AddAddress (1, addr0_2);

  // Print addresses
  NS_LOG_INFO ("Addresses assigned:");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv6> ipv6 = node->GetObject<Ipv6> ();
      for (uint32_t j = 0; j < ipv6->GetNInterfaces (); ++j)
        {
          Ipv6InterfaceAddress addr = ipv6->GetAddress (j, 1);
          NS_LOG_INFO ("Node " << i << " Interface " << j << " Address " << addr.GetAddress ());
        }
    }

  Ptr<Node> routerPtr = router.Get (0);
  Ptr<Ipv6> ipv6RouterPtr = routerPtr->GetObject<Ipv6> ();

  for (uint32_t j = 0; j < ipv6RouterPtr->GetNInterfaces (); ++j)
    {
      Ipv6InterfaceAddress addr = ipv6RouterPtr->GetAddress (j, 1);
      NS_LOG_INFO ("Router Interface " << j << " Address " << addr.GetAddress ());
    }

  NS_LOG_INFO ("Create Router Advertisement daemon.");
  RadvdHelper radvd0;
  radvd0.SetPrefix (Ipv6Prefix ("2001:1::/64"), true);
  radvd0.SetPrefix (Ipv6Prefix ("2001:ABCD::/64"), true);
  radvd0.Install (n0Router.Get (1));

  RadvdHelper radvd1;
  radvd1.SetPrefix (Ipv6Prefix ("2001:2::/64"), true);
  radvd1.Install (n1Router.Get (1));

  NS_LOG_INFO ("Create ping application from n0 to n1.");
  Ping6Helper ping6;
  ping6.SetRemote (i1.GetAddress (0, 1));
  ping6.SetIfIndex (i0.GetInterfaceIndex (0));
  ping6.SetAttribute ("Verbose", BooleanValue (verbose));

  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  Ipv6StaticRoutingHelper staticRouting;
  Ptr<Ipv6StaticRouting> routing = staticRouting.GetStaticRouting (routerPtr->GetObject<Ipv6> ());
  routing->SetDefaultRoute (addr1, 2, 1);

  NS_LOG_INFO ("Enable global routing.");
  Ipv6GlobalRoutingHelper::PopulateRoutingTables ();

  // Tracing
  if (tracing)
    {
      csma0.EnablePcap ("radvd-two-prefix", nd0.Get (0), true);
      csma1.EnablePcap ("radvd-two-prefix", nd1.Get (0), true);
    }

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("radvd-two-prefix.tr");
  csma0.EnableAsciiAll (stream);
  csma1.EnableAsciiAll (stream);

  Simulator::Stop (Seconds (10.0));

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();

  NS_LOG_INFO ("Done.");
  Simulator::Destroy ();

  return 0;
}