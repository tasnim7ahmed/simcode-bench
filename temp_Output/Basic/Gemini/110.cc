#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/drop-tail-queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ping6Example");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nPackets = 5;

  CommandLine cmd;
  cmd.AddValue ("nPackets", "Number of packets sent", nPackets);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_ALL);
    }

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue (100));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (0)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (Ipv6Address ("2001:db8::2"), 0);
  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (1)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (Ipv6Address ("2001:db8::1"), 0);

  Ptr<Node> appSource = nodes.Get (0);
  Ptr<Node> appSink = nodes.Get (1);

  uint16_t echoPort = 9;
  Ping6Helper ping6 (interfaces.GetAddress (1, 1));
  ping6.SetAttribute ("Verbose", BooleanValue (verbose));
  ping6.SetAttribute ("NumberOfPackets", UintegerValue (nPackets));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1)));
  ApplicationContainer apps = ping6.Install (appSource);
  apps.Start (Seconds (2));
  apps.Stop (Seconds (10));

  csma.EnablePcapAll ("ping6");
  csma.EnableQueueDisc ("ns3::FifoQueueDisc", "ping6-queue-disc", true);

  Simulator::Stop (Seconds (11));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}