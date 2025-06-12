#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-header.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/uinteger.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer routerNodes;
  routerNodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer staRouterDevs;
  staRouterDevs = p2p.Install(staNodes.Get(0), routerNodes.Get(0));
  NetDeviceContainer routerStaDevs;
  routerStaDevs = p2p.Install(routerNodes.Get(1), staNodes.Get(1));
  NetDeviceContainer routerRouterDevs;
  routerRouterDevs = p2p.Install(routerNodes.Get(0), routerNodes.Get(1));

  InternetStackHelper internet;
  internet.Install(staNodes);
  internet.Install(routerNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer staRouterIfaces = ipv6.Assign(staRouterDevs);

  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer routerStaIfaces = ipv6.Assign(routerStaDevs);

  ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer routerRouterIfaces = ipv6.Assign(routerRouterDevs);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> sta1Routing = ipv6RoutingHelper.GetOrCreateRouting(staNodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
  sta1Routing->SetDefaultRoute(staRouterIfaces.GetAddress(1, 0), 0);

  Ptr<Ipv6StaticRouting> sta2Routing = ipv6RoutingHelper.GetOrCreateRouting(staNodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
  sta2Routing->SetDefaultRoute(routerStaIfaces.GetAddress(0, 0), 0);

  Ptr<Ipv6StaticRouting> r1Routing = ipv6RoutingHelper.GetOrCreateRouting(routerNodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
  r1Routing->AddRouteToNetwork(Ipv6Address("2001:2::"), Ipv6Prefix(64), routerRouterIfaces.GetAddress(1, 0), 1);

  Ipv6EchoClientHelper echoClientHelper(routerStaIfaces.GetAddress(1, 0), 1);
  echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1)));
  echoClientHelper.SetAttribute("MaxPackets", UintegerValue(1));
  ApplicationContainer clientApp = echoClientHelper.Install(staNodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(5.0));

  Simulator::Stop(Seconds(10.0));

  p2p.EnablePcapAll("icmpv6-redirect");

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}