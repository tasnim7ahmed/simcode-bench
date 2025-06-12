#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer router;
  router.Create(1);

  NodeContainer n0r = NodeContainer(nodes.Get(0), router.Get(0));
  NodeContainer rn1 = NodeContainer(router.Get(0), nodes.Get(1));

  CsmaHelper csma01;
  csma01.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  csma01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  NetDeviceContainer d0d2 = csma01.Install(n0r);

  CsmaHelper csma23;
  csma23.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  csma23.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  NetDeviceContainer d2d3 = csma23.Install(rn1);

  InternetStackHelper stack;
  stack.Install(nodes);
  stack.Install(router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i0i2 = ipv6.Assign(d0d2);

  ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i2i3 = ipv6.Assign(d2d3);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting(router.Get(0)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:1::/64"), i2i3.GetAddress(0), 2, i2i3.GetNetDevice(0));
  staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:2::/64"), i0i2.GetAddress(1), 1, i0i2.GetNetDevice(1));

  ipv6RoutingHelper.SetDefaultRoute(router.Get(0), i0i2.GetAddress(1), 1);
  ipv6RoutingHelper.SetDefaultRoute(nodes.Get(0), i0i2.GetAddress(1), 1);
  ipv6RoutingHelper.SetDefaultRoute(nodes.Get(1), i2i3.GetAddress(0), 1);

  Ping6Helper ping6(i2i3.GetAddress(1));
  ping6.SetAttribute("Verbose", BooleanValue(true));
  ApplicationContainer apps = ping6.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));

  AsciiTraceHelper ascii;
  csma01.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));
  csma23.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));
  csma01.EnablePcapAll("ping6", false);
  csma23.EnablePcapAll("ping6", false);

  Simulator::Run();

  Ptr<Ipv6RoutingTable> routingTable = nodes.Get(0)->GetObject<Ipv6>()->GetRoutingTable();
  routingTable->PrintRoutingTable(std::cout);

  Simulator::Destroy();

  return 0;
}