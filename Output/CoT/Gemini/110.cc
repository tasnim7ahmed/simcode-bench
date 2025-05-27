#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/drop-tail-queue.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetStaticRouting(nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
  staticRouting->SetDefaultRoute(interfaces.GetAddress(1, 1), 0);
  staticRouting = Ipv6RoutingHelper::GetStaticRouting(nodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
  staticRouting->SetDefaultRoute(interfaces.GetAddress(0, 1), 0);

  V4Ping6Helper ping6;
  ping6.SetRemote(interfaces.GetAddress(1, 1));
  ping6.SetAttribute("Verbose", BooleanValue(true));

  ApplicationContainer apps = ping6.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  csma.EnablePcapAll("ping6", false);

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("ping6.tr");
  csma.EnableAsciiAll(stream);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}