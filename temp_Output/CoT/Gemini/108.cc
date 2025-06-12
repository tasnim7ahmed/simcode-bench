#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-header.h"
#include "ns3/ping6-application.h"
#include "ns3/ipv6-address.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Icmpv6Redirect");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("Icmpv6Redirect", AttributeValue (BooleanValue (true)));

  NodeContainer nodes;
  nodes.Create (4);

  NodeContainer sta1Node = NodeContainer (nodes.Get (0));
  NodeContainer r1Node = NodeContainer (nodes.Get (1));
  NodeContainer r2Node = NodeContainer (nodes.Get (2));
  NodeContainer sta2Node = NodeContainer (nodes.Get (3));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer sta1R1Devices = pointToPoint.Install (sta1Node, r1Node);
  NetDeviceContainer r1R2Devices = pointToPoint.Install (r1Node, r2Node);
  NetDeviceContainer sta2R2Devices = pointToPoint.Install (sta2Node, r2Node);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer sta1R1Interfaces = ipv6.Assign (sta1R1Devices);

  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer r1R2Interfaces = ipv6.Assign (r1R2Devices);

  ipv6.SetBase (Ipv6Address ("2001:db8:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer sta2R2Interfaces = ipv6.Assign (sta2R2Devices);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> sta1Routing = ipv6RoutingHelper.GetStaticRouting (sta1Node->GetObject<Ipv6> ());
  sta1Routing->AddDefaultRoute (sta1R1Interfaces.GetAddress (1, 0), 0);

  Ptr<Ipv6StaticRouting> r1Routing = ipv6RoutingHelper.GetStaticRouting (r1Node->GetObject<Ipv6> ());
  r1Routing->AddHostRouteToNetwork (sta2R2Interfaces.GetAddress (0, 0), r1R2Interfaces.GetAddress (1,0), 1);

  Ptr<Ipv6StaticRouting> sta2Routing = ipv6RoutingHelper.GetStaticRouting (sta2Node->GetObject<Ipv6> ());
  sta2Routing->AddDefaultRoute (sta2R2Interfaces.GetAddress (1, 0), 0);

  Ping6 application (sta2R2Interfaces.GetAddress (0, 0));
  application.SetIfIndex (sta1R1Interfaces.GetInterfaceIndex (0,0));
  application.SetStartTime (Seconds (1.0));
  application.SetStopTime (Seconds (10.0));
  sta1Node->AddApplication (&application);

  Simulator::Stop (Seconds (12.0));

  pointToPoint.EnablePcapAll ("icmpv6-redirect");

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}