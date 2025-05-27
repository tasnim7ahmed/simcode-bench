#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/icmpv6-l4-protocol.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
  LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create(4);

  NodeContainer sta1r1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer r1r2 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer r2sta2 = NodeContainer(nodes.Get(2), nodes.Get(3));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d_sta1r1 = p2p.Install(sta1r1);
  NetDeviceContainer d_r1r2 = p2p.Install(r1r2);
  NetDeviceContainer d_r2sta2 = p2p.Install(r2sta2);

  InternetStackHelper internetv6;
  internetv6.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i_sta1r1 = ipv6.Assign(d_sta1r1);
  ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i_r1r2 = ipv6.Assign(d_r1r2);
  ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i_r2sta2 = ipv6.Assign(d_r2sta2);

  i_sta1r1.SetForwarding(1, true);
  i_r1r2.SetForwarding(0, true);
  i_r1r2.SetForwarding(1, true);
  i_r2sta2.SetForwarding(0, true);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRoutingSta1 = ipv6RoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv6> ());
  staticRoutingSta1->AddDefaultRoute(i_sta1r1.GetAddress(1), 0);

  Ptr<Ipv6StaticRouting> staticRoutingR1 = ipv6RoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv6> ());
  staticRoutingR1->AddRouteToNetwork(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64), i_r1r2.GetAddress(1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingSta2 = ipv6RoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv6> ());
  staticRoutingSta2->AddDefaultRoute(i_r2sta2.GetAddress(1), 0);

  uint16_t echoPort = 9;
  V6PingHelper echoClientHelper(i_r2sta2.GetAddress(0));
  echoClientHelper.SetAttribute("Verbose", BooleanValue(true));
  ApplicationContainer echoClient = echoClientHelper.Install(nodes.Get(0));
  echoClient.Start(Seconds(1.0));
  echoClient.Stop(Seconds(5.0));

  Simulator::Schedule(Seconds(3.0), [](){
    Ptr<Icmpv6L4Protocol> icmpv6_r1 = DynamicCast<Icmpv6L4Protocol>(Simulator::GetNode(1)->GetObject<Ipv6>()->GetProtocol(58));
    icmpv6_r1->EnableSendIcmpv6Redirect(true);
  });

  p2p.EnablePcapAll("icmpv6-redirect", false);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}