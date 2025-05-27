#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIPv6");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv6Queue", LOG_LEVEL_ALL);
  LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  NodeContainer n0r = NodeContainer (nodes.Get (0), router.Get (0));
  NodeContainer n1r = NodeContainer (nodes.Get (1), router.Get (0));

  CsmaHelper csmaN0R;
  csmaN0R.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csmaN0R.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer ndN0R = csmaN0R.Install (n0r);

  CsmaHelper csmaN1R;
  csmaN1R.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csmaN1R.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer ndN1R = csmaN1R.Install (n1r);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);
  internetv6.Install (router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iN0R = ipv6.Assign (ndN0R);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iN1R = ipv6.Assign (ndN1R);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetIpv6StaticRouting (router.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddRoutingTableEntry (Ipv6Address ("2001:db8:0:1::/64"), 1, 0);
  staticRouting->AddRoutingTableEntry (Ipv6Address ("2001:db8:0:2::/64"), 2, 0);

  staticRouting = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->SetDefaultRoute (iN0R.GetAddress (1,0), 1, 0);

  staticRouting = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRouting->SetDefaultRoute (iN1R.GetAddress (1,0), 2, 0);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (iN1R.GetAddress (0,0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (2000));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing)
    {
      csmaN0R.EnablePcapAll ("fragmentation-ipv6");
      csmaN1R.EnablePcapAll ("fragmentation-ipv6");
    }

  AsciiTraceHelper ascii;
  csmaN0R.EnableAsciiAll (ascii.CreateFileStream ("fragmentation-ipv6.tr"));
  csmaN1R.EnableAsciiAll (ascii.CreateFileStream ("fragmentation-ipv6.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}