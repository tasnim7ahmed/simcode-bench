#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6TwoMtu");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1));

  NodeContainer nodes;
  nodes.Create (5);

  NodeContainer n0r = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer rn1 = NodeContainer (nodes.Get (2), nodes.Get (3));

  CsmaHelper csma0r;
  csma0r.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma0r.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  csma0r.SetDeviceAttribute ("Mtu", UintegerValue (5000));

  NetDeviceContainer d0r = csma0r.Install (n0r);

  CsmaHelper csmar1;
  csmar1.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csmar1.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  csmar1.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  NetDeviceContainer dr1 = csmar1.Install (rn1);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0r = ipv6.Assign (d0r);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ir1 = ipv6.Assign (dr1);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (1)->GetObject<Ipv6> ()->GetRoutingProtocol (0));
  staticRouting->SetDefaultRoute (i0r.GetAddress (1,0), 0);
  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (3)->GetObject<Ipv6> ()->GetRoutingProtocol (0));
  staticRouting->SetDefaultRoute (ir1.GetAddress (0,0), 0);

  Ipv6Address dstAddr = ir1.GetAddress (1,0);
  Ptr<Ipv6StaticRouting> staticRoutingR = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (2)->GetObject<Ipv6> ()->GetRoutingProtocol (0));
  staticRoutingR->AddHostRouteTo (dstAddr, i0r.GetAddress (0,0), 1);
  staticRoutingR->AddHostRouteTo (i0r.GetAddress (0,0), ir1.GetAddress (1,0), 1);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (ir1.GetAddress (1,0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  csma0r.EnablePcapAll ("fragmentation-ipv6-two-mtu", false);
  csmar1.EnablePcapAll ("fragmentation-ipv6-two-mtu", false);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}