#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable("Ipv6RoutingTable", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer router;
  router.Create(1);

  CsmaHelper csma01;
  csma01.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer nd0r = csma01.Install(NodeContainer(nodes.Get(0), router.Get(0)));

  CsmaHelper csma11;
  csma11.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma11.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer nd1r = csma11.Install(NodeContainer(nodes.Get(1), router.Get(0)));

  InternetStackHelper internetv6;
  internetv6.Install(nodes);
  internetv6.Install(router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8:0:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i0r = ipv6.Assign(nd0r);

  ipv6.SetBase(Ipv6Address("2001:db8:0:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i1r = ipv6.Assign(nd1r);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting(router.Get(0)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:0:1::1"), 1, i0r.GetNetDevice(1));
  staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:0:2::1"), 1, i1r.GetNetDevice(1));

  ipv6RoutingHelper.SetDefaultRoute(router.Get(0)->GetObject<Ipv6> (), i0r.GetAddress(1,0));
  ipv6RoutingHelper.SetDefaultRoute(nodes.Get(0)->GetObject<Ipv6> (), i0r.GetAddress(1,0));
  ipv6RoutingHelper.SetDefaultRoute(nodes.Get(1)->GetObject<Ipv6> (), i1r.GetAddress(1,0));

  ApplicationContainer pingApp;
  uint32_t packetSize = 1024;
  uint32_t maxPackets = 10;
  Time interPacketInterval = Seconds(1.0);
  Ping6Helper ping6;

  ping6.SetRemote(i1r.GetAddress(0,0));
  ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
  ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  ping6.SetAttribute("Interval", TimeValue(interPacketInterval));

  pingApp = ping6.Install(nodes.Get(0));
  pingApp.Start(Seconds(2.0));
  pingApp.Stop(Seconds(10.0));

  AsciiTraceHelper ascii;
  csma01.EnableAsciiAll(ascii.CreateFileStream("csma01.tr"));
  csma11.EnableAsciiAll(ascii.CreateFileStream("csma11.tr"));

  csma01.EnablePcapAll("csma01", false);
  csma11.EnablePcapAll("csma11", false);

  Simulator::Stop(Seconds(11.0));

  std::cout << "\nIPv6 Routing Table for Node 0:" << std::endl;
  Ptr<Ipv6RoutingTable> routingTable = nodes.Get(0)->GetObject<Ipv6>()->GetRoutingTable();
  routingTable->Print();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}