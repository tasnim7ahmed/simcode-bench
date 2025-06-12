#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include "ns3/queue-disc.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefix");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetAttribute("Ipv6L3Protocol", "Forwarding", BooleanValue(true));
  LogComponent::SetAttribute("Queue", "MaxSize", StringValue("20p"));

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer router;
  router.Create(1);

  CsmaHelper csma0;
  csma0.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  csma0.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer nd0 = csma0.Install(NodeContainer(nodes.Get(0), router.Get(0)));

  CsmaHelper csma1;
  csma1.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  csma1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer nd1 = csma1.Install(NodeContainer(nodes.Get(1), router.Get(0)));

  InternetStackHelper internet;
  internet.Install(nodes);
  internet.Install(router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i0 = ipv6.Assign(nd0);

  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i1 = ipv6.Assign(nd1);

  Ipv6StaticRoutingHelper staticRouting;
  Ptr<Ipv6StaticRouting> routing = staticRouting.GetOrCreateRouting(router.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
  routing->SetDefaultRoute(i1.GetAddress(1), 1);

  Ptr<Node> radvdServer = router.Get(0);
  Ptr<Radvd> radvd = radvdServer->GetObject<Radvd>();
  radvd->SetManagedFlag(true);
  radvd->SetOtherConfigFlag(true);

  radvd->AddInterface(1, i0.GetAddress(1));
  radvd->AddInterface(2, i1.GetAddress(1));

  radvd->AddPrefix(1, Ipv6Prefix("2001:1::/64"));
  radvd->AddPrefix(1, Ipv6Prefix("2001:ABCD::/64"));
  radvd->AddPrefix(2, Ipv6Prefix("2001:2::/64"));

  nodes.Get(0)->GetObject<Ipv6>()->SetRouterSolicitationDelay(Seconds(0.1));
  nodes.Get(1)->GetObject<Ipv6>()->SetRouterSolicitationDelay(Seconds(0.1));

  Icmpv6EchoClientHelper ping(i1.GetAddress(0), 1);
  ping.SetAttribute("Interval", TimeValue(Seconds(1)));
  ping.SetAttribute("MaxPackets", UintegerValue(5));
  ApplicationContainer apps = ping.Install(nodes.Get(0));
  apps.Start(Seconds(5));
  apps.Stop(Seconds(15));

  for (uint32_t i = 0; i < nd0.GetN(); ++i) {
        Ptr<Queue> queue = StaticCast<PointToPointNetDevice>(nd0.Get(i))->GetQueue();
        if (queue != nullptr) {
            Simulator::Schedule(Seconds(0.1), [queue]() {
                queue->TraceConnectWithoutContext("BytesInQueue", "radvd-two-prefix.tr");
            });
        }
  }
  for (uint32_t i = 0; i < nd1.GetN(); ++i) {
        Ptr<Queue> queue = StaticCast<PointToPointNetDevice>(nd1.Get(i))->GetQueue();
        if (queue != nullptr) {
            Simulator::Schedule(Seconds(0.1), [queue]() {
                queue->TraceConnectWithoutContext("BytesInQueue", "radvd-two-prefix.tr");
            });
        }
  }

  AsciiTraceHelper ascii;
  csma0.EnableAsciiAll(ascii.CreateFileStream("radvd-two-prefix.tr"));
  csma1.EnableAsciiAll(ascii.CreateFileStream("radvd-two-prefix.tr"));

  Simulator::Stop(Seconds(20));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}