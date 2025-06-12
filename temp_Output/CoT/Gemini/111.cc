#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-module.h"
#include "ns3/ping6-application.h"
#include "ns3/net-device-queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  CsmaHelper csma0;
  csma0.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma0.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));

  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma1.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));

  NetDeviceContainer nd0 = csma0.Install (NodeContainer (nodes.Get (0), router.Get (0)));
  NetDeviceContainer nd1 = csma1.Install (NodeContainer (nodes.Get (1), router.Get (0)));

  InternetStackHelper stack;
  stack.Install (nodes);
  stack.Install (router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0 = ipv6.Assign (nd0);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1 = ipv6.Assign (nd1);

  Ipv6Address routerAddress0 = i0.GetAddress (1, 0);
  Ipv6Address routerAddress1 = i1.GetAddress (1, 0);

  Ptr<NetDevice> dev0 = nd0.Get (1);
  Ptr<NetDevice> dev1 = nd1.Get (1);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting (router.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddRoutingTableEntry (Ipv6Address ("2001:1::"), 64, dev0, Ipv6Address ("::"), -1, 0);
  staticRouting->AddRoutingTableEntry (Ipv6Address ("2001:2::"), 64, dev1, Ipv6Address ("::"), -1, 0);

  RadvdHelper radvdHelper;
  radvdHelper.SetPrefix (Ipv6Prefix ("2001:1::/64"), true);
  radvdHelper.SetAdvSendAdvert (true);
  radvdHelper.Install (router.Get (0), nd0.Get (0));

  radvdHelper.SetPrefix (Ipv6Prefix ("2001:2::/64"), true);
  radvdHelper.SetAdvSendAdvert (true);
  radvdHelper.Install (router.Get (0), nd1.Get (0));

  uint32_t echoPort = 9;
  Ping6ApplicationHelper ping6;
  ping6.SetRemote (i1.GetAddress (0, 0));
  ping6.SetAttribute ("Verbose", BooleanValue (true));

  ApplicationContainer p = ping6.Install (nodes.Get (0));
  p.Start (Seconds (2));
  p.Stop (Seconds (10));

  Simulator::Stop (Seconds (11));

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("radvd.tr");
  csma0.EnableAsciiAll (stream);
  csma1.EnableAsciiAll (stream);

  for (uint32_t i = 0; i < nd0.GetN(); ++i)
  {
    Ptr<NetDevice> nd = nd0.Get (i);
    Ptr<Queue> queue = nd->GetQueue ();
    if (queue != nullptr)
    {
      queue->TraceConnectWithoutContext ("Drop", "radvd.tr", MakeBoundCallback (&AsciiTraceHelper::QueueDropTracer, stream));
    }
  }

    for (uint32_t i = 0; i < nd1.GetN(); ++i)
  {
    Ptr<NetDevice> nd = nd1.Get (i);
    Ptr<Queue> queue = nd->GetQueue ();
    if (queue != nullptr)
    {
      queue->TraceConnectWithoutContext ("Drop", "radvd.tr", MakeBoundCallback (&AsciiTraceHelper::QueueDropTracer, stream));
    }
  }

  nodes.Get(0)->TraceConnectWithoutContext("MacRx", "radvd.tr", MakeBoundCallback (&AsciiTraceHelper::MacRxTracer, stream));
  nodes.Get(1)->TraceConnectWithoutContext("MacRx", "radvd.tr", MakeBoundCallback (&AsciiTraceHelper::MacRxTracer, stream));
  router.Get(0)->TraceConnectWithoutContext("MacRx", "radvd.tr", MakeBoundCallback (&AsciiTraceHelper::MacRxTracer, stream));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}