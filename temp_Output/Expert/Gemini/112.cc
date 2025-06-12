#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-interface-address.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/radvd-interface.h"
#include "ns3/ndisc-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/packet.h"
#include "ns3/queue.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable("CsmaChannel", LOG_LEVEL_INFO);
  LogComponentEnable("QueueDisc", LOG_LEVEL_ALL);
  LogComponentEnable("Radvd", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer routerNode;
  routerNode.Create(1);

  NodeContainer n0Router = NodeContainer(nodes.Get(0), routerNode.Get(0));
  NodeContainer n1Router = NodeContainer(nodes.Get(1), routerNode.Get(0));

  CsmaHelper csma01, csma12;
  csma01.SetChannelAttribute("DataRate", DataRateValue(1000000));
  csma01.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));
  csma12.SetChannelAttribute("DataRate", DataRateValue(1000000));
  csma12.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

  NetDeviceContainer devices01 = csma01.Install(n0Router);
  NetDeviceContainer devices12 = csma12.Install(n1Router);

  InternetStackHelper internetv6;
  internetv6.Install(nodes);
  internetv6.Install(routerNode);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces01 = ipv6.Assign(devices01);

  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces12 = ipv6.Assign(devices12);

  Ptr<Node> router = routerNode.Get(0);
  Ptr<NetDevice> device0 = devices01.Get(1);
  Ptr<NetDevice> device1 = devices12.Get(1);
  uint32_t ifIndex0 = router->GetNetDeviceIndex(device0);
  uint32_t ifIndex1 = router->GetNetDeviceIndex(device1);
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting(router);
  staticRouting->AddRoute(Ipv6Address("2001:1::"), 64, ifIndex0);
  staticRouting->AddRoute(Ipv6Address("2001:2::"), 64, ifIndex1);
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      Ptr<Ipv6StaticRouting> staticRoutingNode = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(i));
      staticRoutingNode->SetDefaultRoute("::", 0, i == 0 ? ifIndex0 : ifIndex1);
  }

  interfaces01.GetAddress(0,1).SetDadState(DAD_STATE_INVALID);
  interfaces12.GetAddress(0,1).SetDadState(DAD_STATE_INVALID);

  Ptr<RadvdInterface> radvd0 = CreateObject<RadvdInterface>();
  radvd0->SetAdvSendAdvert(true);
  radvd0->AddPrefix(Ipv6Prefix("2001:1::/64"), RadvdInterface::INFINITE_LIFETIME, RadvdInterface::INFINITE_LIFETIME);
  radvd0->AddPrefix(Ipv6Prefix("2001:ABCD::/64"), RadvdInterface::INFINITE_LIFETIME, RadvdInterface::INFINITE_LIFETIME);
  router->GetObject<Ipv6L3Protocol>()->SetRadvdInterface(ifIndex0, radvd0);

  Ptr<RadvdInterface> radvd1 = CreateObject<RadvdInterface>();
  radvd1->SetAdvSendAdvert(true);
  radvd1->AddPrefix(Ipv6Prefix("2001:2::/64"), RadvdInterface::INFINITE_LIFETIME, RadvdInterface::INFINITE_LIFETIME);
  router->GetObject<Ipv6L3Protocol>()->SetRadvdInterface(ifIndex1, radvd1);

  Ping6Helper ping6;
  ping6.SetRemote(interfaces12.GetAddress(0,1).GetAddress());
  ping6.SetIfIndex(1);
  ping6.SetCount(5);
  ping6.SetInterval(Seconds(1));
  ApplicationContainer apps = ping6.Install(nodes.Get(0));
  apps.Start(Seconds(2));
  apps.Stop(Seconds(10));

  csma01.EnablePcapAll("radvd-two-prefix", false);
  csma12.EnablePcapAll("radvd-two-prefix", false);

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("radvd-two-prefix.tr");
  csma01.EnableAsciiAll(stream);
  csma12.EnableAsciiAll(stream);

  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}