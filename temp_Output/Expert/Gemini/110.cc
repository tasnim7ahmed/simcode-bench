#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  InternetStackHelper internetv6;
  internetv6.Install(nodes);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
  staticRouting->SetDefaultRoute(interfaces.GetAddress(1, 1), 0);
  staticRouting = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(nodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
  staticRouting->SetDefaultRoute(interfaces.GetAddress(0, 1), 0);

  uint32_t packetSize = 1024;
  uint32_t maxPackets = 10;
  Time interPacketInterval = Seconds(1.0);

  Ping6Helper ping6;
  ping6.SetRemote(interfaces.GetAddress(1, 1));
  ping6.SetIfIface(interfaces.GetAddress(0, 1));
  ping6.SetAttribute("Verbose", BooleanValue(true));
  ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
  ping6.SetAttribute("Size", UintegerValue(packetSize));
  ping6.SetAttribute("Count", UintegerValue(maxPackets));

  ApplicationContainer apps = ping6.Install(nodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(2.0 + maxPackets + 1));

  csma.EnableQueueDiscTracing("ping6.tr", devices);
  devices.Get(0)->TraceConnectWithoutContext("PhyRxDrop", "ping6.tr", MakeCallback(&CsmaHelper::PhyRxDropEvent));
  devices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", "ping6.tr", MakeCallback(&CsmaHelper::PhyRxDropEvent));
  devices.Get(0)->TraceConnectWithoutContext("PhyRxEnd", "ping6.tr", MakeCallback(&CsmaHelper::PhyRxEndEvent));
  devices.Get(1)->TraceConnectWithoutContext("PhyRxEnd", "ping6.tr", MakeCallback(&CsmaHelper::PhyRxEndEvent));

  Simulator::Stop(Seconds(2.0 + maxPackets + 2));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}