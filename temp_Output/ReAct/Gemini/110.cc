#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1));

  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (0)->GetObject <Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (Ipv6Address ("2001:db8::2"), 0);
  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (1)->GetObject <Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (Ipv6Address ("2001:db8::1"), 0);

  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  Time interPacketInterval = Seconds(1.0);

  V6PingHelper ping(interfaces.GetAddress(1));
  ping.SetAttribute("Verbose", BooleanValue(true));
  ping.SetAttribute("Count", UintegerValue(numPackets));
  ping.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer apps = ping.Install(nodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(12.0));

  csma.EnablePcapAll("ping6", false);

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("ping6.tr");
  csma.EnableAsciiAll(stream);

  Simulator::Stop(Seconds(15.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}