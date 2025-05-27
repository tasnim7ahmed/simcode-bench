#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationTwoMtu");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1));

  NodeContainer nodes;
  nodes.Create (5);

  NodeContainer n0r = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer rn1 = NodeContainer (nodes.Get (2), nodes.Get (3));

  CsmaHelper csmaN0R;
  csmaN0R.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csmaN0R.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csmaN0R.SetDeviceAttribute ("Mtu", UintegerValue (5000));

  CsmaHelper csmaRN1;
  csmaRN1.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csmaRN1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csmaRN1.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  NetDeviceContainer dN0R = csmaN0R.Install (n0r);
  NetDeviceContainer dRN1 = csmaRN1.Install (rn1);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iN0R = ipv6.Assign (dN0R);
  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iRN1 = ipv6.Assign (dRN1);

  // Set up static routes
  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (2)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (iRN1.GetAddress (1, 0), 0);

  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (1)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (iN0R.GetAddress (1, 0), 0);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (iRN1.GetAddress (1, 0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Add trace sinks
  csmaN0R.EnablePcap ("fragmentation-ipv6-two-mtu-n0r", dN0R.Get (0), true);
  csmaRN1.EnablePcap ("fragmentation-ipv6-two-mtu-rn1", dRN1.Get (0), true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}