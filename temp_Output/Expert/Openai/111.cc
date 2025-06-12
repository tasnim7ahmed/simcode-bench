#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/radvd-helper.h"
#include "ns3/ipv6-address-generator.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/queue-disc-helper.h"
#include "ns3/ipv6-ping-helper.h"

using namespace ns3;

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream out("radvd.tr", std::ios_base::app);
  out << Simulator::Now().GetSeconds() << " " << context << " Received Packet of size " << packet->GetSize() << std::endl;
}

void
QueueTrace(std::string context, Ptr<const QueueDiscItem> item)
{
  static std::ofstream out("radvd.tr", std::ios_base::app);
  out << Simulator::Now().GetSeconds() << " " << context << " Queued Packet of size " << item->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("Ipv6RadvdApplication", LOG_LEVEL_INFO);

  NodeContainer n0, n1, r;
  n0.Create(1);
  n1.Create(1);
  r.Create(1);

  NodeContainer n0r, n1r;
  n0r.Add(n0.Get(0));
  n0r.Add(r.Get(0));
  n1r.Add(n1.Get(0));
  n1r.Add(r.Get(0));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

  NetDeviceContainer nd0r = csma.Install(n0r);
  NetDeviceContainer nd1r = csma.Install(n1r);

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall(false);
  internetv6.Install(n0);
  internetv6.Install(n1);
  internetv6.Install(r);

  Ipv6AddressHelper ipv6_0;
  ipv6_0.SetBase("2001:1::", 64);
  Ipv6InterfaceContainer i0r = ipv6_0.Assign(nd0r);
  i0r.SetForwarding(1, true);
  i0r.SetDefaultRouteInAllNodes(0);

  Ipv6AddressHelper ipv6_1;
  ipv6_1.SetBase("2001:2::", 64);
  Ipv6InterfaceContainer i1r = ipv6_1.Assign(nd1r);
  i1r.SetForwarding(1, true);
  i1r.SetDefaultRouteInAllNodes(0);

  RadvdHelper radvdHelper;
  Ptr<Radvd> radvd = radvdHelper.Create(r.Get(0));

  RadvdInterface interface0;
  interface0.SetInterfaceId(r->GetDevice(0)->GetIfIndex());
  interface0.AddPrefix(RadvdPrefix("2001:1::", 64));
  interface0.SetAdvSendAdvert(true);

  RadvdInterface interface1;
  interface1.SetInterfaceId(r->GetDevice(1)->GetIfIndex());
  interface1.AddPrefix(RadvdPrefix("2001:2::", 64));
  interface1.SetAdvSendAdvert(true);

  radvd->AddInterface(interface0);
  radvd->AddInterface(interface1);

  r.Get(0)->AddApplication(radvd);
  radvd->SetStartTime(Seconds(1.0));
  radvd->SetStopTime(Seconds(20.0));

  Ipv6PingHelper ping6(i1r.GetAddress(0, 1));
  ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ping6.SetAttribute("Size", UintegerValue(56));
  ping6.SetAttribute("Count", UintegerValue(5));
  ApplicationContainer apps = ping6.Install(n0.Get(0));
  apps.Start(Seconds(3.0));
  apps.Stop(Seconds(20.0));

  // Trace packet receptions (Rx)
  nd1r.Get(0)->TraceConnect("MacRx", "n1", MakeCallback(&RxTrace));
  nd0r.Get(0)->TraceConnect("MacRx", "n0", MakeCallback(&RxTrace));

  // Trace queue (packet enqueue events)
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
  tch.Install(nd0r);
  tch.Install(nd1r);

  Ptr<QueueDisc> q0 = r.Get(0)->GetObject<TrafficControlLayer>()->GetQueueDiscOnDevice(r.Get(0)->GetDevice(0));
  Ptr<QueueDisc> q1 = r.Get(0)->GetObject<TrafficControlLayer>()->GetQueueDiscOnDevice(r.Get(0)->GetDevice(1));

  if (q0)
    q0->TraceConnect("Enqueue", "R0", MakeCallback(&QueueTrace));
  if (q1)
    q1->TraceConnect("Enqueue", "R1", MakeCallback(&QueueTrace));

  Simulator::Stop(Seconds(21.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}