#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/multicast-routing-protocol.h"
#include "ns3/on-off-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(5);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices02 = p2p.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices03 = p2p.Install(nodes.Get(0), nodes.Get(3));
  NetDeviceContainer devices04 = p2p.Install(nodes.Get(0), nodes.Get(4));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces03 = address.Assign(devices03);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces04 = address.Assign(devices04);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ipv4Address multicastGroup("225.1.2.3");
  GlobalRouteManager::SetIpv4MulticastRouting(true);

  Ptr<Ipv4MulticastRoute> multicastRoute = Create<Ipv4MulticastRoute>();
  multicastRoute->SetOrigin(interfaces01.GetAddress(0));
  multicastRoute->SetSource(Ipv4Address::GetZero());
  multicastRoute->SetGroup(multicastGroup);

  Ipv4MulticastRouteVector routes;
  routes.push_back(multicastRoute);
  Ipv4MulticastRoutingHelper::AddStaticRoutes(routes);

  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4StaticRouting> routingTable0 = staticRouting.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
  routingTable0->AddMulticastRoute(Ipv4Address::GetZero(), multicastGroup, interfaces01.GetAddress(0), 1);
  routingTable0->AddMulticastRoute(Ipv4Address::GetZero(), multicastGroup, interfaces02.GetAddress(0), 1);
  routingTable0->AddMulticastRoute(Ipv4Address::GetZero(), multicastGroup, interfaces03.GetAddress(0), 1);
  routingTable0->AddMulticastRoute(Ipv4Address::GetZero(), multicastGroup, interfaces04.GetAddress(0), 1);

  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinks;
  sinks.Add(sinkHelper.Install(nodes.Get(1)));
  sinks.Add(sinkHelper.Install(nodes.Get(2)));
  sinks.Add(sinkHelper.Install(nodes.Get(3)));
  sinks.Add(sinkHelper.Install(nodes.Get(4)));
  sinks.Start(Seconds(1.0));

  OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(multicastGroup, 9)));
  onOffHelper.SetConstantRate(DataRate("1Mb/s"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer apps = onOffHelper.Install(nodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  p2p.EnablePcapAll("multicast");

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}