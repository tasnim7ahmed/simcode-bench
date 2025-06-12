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

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::Enable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::Enable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponent::Enable ("Ipv6L3Protocol", LOG_LEVEL_ALL);
  LogComponent::Enable ("Ipv6Queue", LOG_LEVEL_ALL);
  LogComponent::Enable ("RateErrorModel", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0r = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer n1r = NodeContainer (nodes.Get (1), nodes.Get (2));

  CsmaHelper csma01;
  csma01.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma01.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d0d1 = csma01.Install (n0r);

  CsmaHelper csma12;
  csma12.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma12.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d1d2 = csma12.Install (n1r);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv6AddressHelper address;
  address.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1i2 = address.Assign (d1d2);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRouteTo (Ipv6Address ("2001:db8:2::/64"), i0i1.GetAddress (1,0), 1);

  staticRouting = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (2)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRouteTo (Ipv6Address ("2001:db8:1::/64"), i1i2.GetAddress (1,0), 1);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i1i2.GetAddress (1,0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (2000));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Schedule (Seconds(9.0), [](){
    std::cout << "Checking routing table of node 1:" << std::endl;
    Ptr<Ipv6StaticRouting> routing = StaticCast<Ipv6StaticRouting>(nodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
    auto routes = routing->GetRoutingTableCopy();
    for(auto const& route : routes){
        std::cout << "  Destination: " << route.GetDest() << std::endl;
        std::cout << "  Gateway: " << route.GetGateway() << std::endl;
        std::cout << "  Output Interface: " << route.GetOutputTtl() << std::endl;
    }
  });

  csma01.EnablePcap ("fragmentation-ipv6", d0d1.Get (0), true);
  csma12.EnablePcap ("fragmentation-ipv6", d1d2.Get (1), true);

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("fragmentation-ipv6.tr");
  csma01.EnableAsciiAll (stream);
  csma12.EnableAsciiAll (stream);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}