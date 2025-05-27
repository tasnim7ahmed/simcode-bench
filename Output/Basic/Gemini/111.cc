#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-module.h"
#include "ns3/ping6.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("RadvdExample", LOG_LEVEL_INFO);
      LogComponentEnable ("Radvd", LOG_LEVEL_ALL);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_ALL);
    }

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  CsmaHelper csma0;
  csma0.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma0.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer nd0 = csma0.Install (NodeContainer (nodes.Get (0), router.Get (0)));
  NetDeviceContainer nd1 = csma1.Install (NodeContainer (nodes.Get (1), router.Get (0)));

  InternetStackHelper stack;
  stack.Install (nodes);
  stack.Install (router);

  Ipv6AddressHelper address;
  address.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0 = address.Assign (nd0);

  address.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1 = address.Assign (nd1);

  Ipv6Address routerAddress0 = Ipv6Address ("2001:1::1");
  Ipv6Address routerAddress1 = Ipv6Address ("2001:2::1");
  i0.SetForwarding (1, true);
  i1.SetForwarding (1, true);
  i0.SetDefaultRouteInAllNodes(routerAddress0);
  i1.SetDefaultRouteInAllNodes(routerAddress1);

  RadvdHelper radvd0;
  radvd0.SetAdvPrefix (Ipv6Prefix ("2001:1::"), 64);
  radvd0.Install (router.Get (0), nd0.Get (1));

  RadvdHelper radvd1;
  radvd1.SetAdvPrefix (Ipv6Prefix ("2001:2::"), 64);
  radvd1.Install (router.Get (0), nd1.Get (1));

  uint32_t echoPort = 9;
  Ping6Helper ping6;
  ping6.SetRemote (i1.GetAddress (1, 0));
  ping6.SetIf (i0.GetAddress (0, 0));
  ping6.SetAttribute ("Verbose", BooleanValue (true));

  ApplicationContainer p = ping6.Install (nodes.Get (0));
  p.Start (Seconds (1.0));
  p.Stop (Seconds (10.0));

  if (tracing)
    {
      csma0.EnablePcapAll ("radvd", false);
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}