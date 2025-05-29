#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(7);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer p2pDevices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer p2pDevices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer p2pDevices56 = pointToPoint.Install(nodes.Get(5), nodes.Get(6));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer csmaDevices = csma.Install(NodeContainer(nodes.Get(2), nodes.Get(3), nodes.Get(4), nodes.Get(5)));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces02 = address.Assign(p2pDevices02);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces12 = address.Assign(p2pDevices12);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces56 = address.Assign(p2pDevices56);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  OnOffHelper onoff("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(p2pInterfaces56.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate("448Kbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(210));

  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  app = sink.Install(nodes.Get(6));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));

  pointToPoint.EnablePcapAll("mixed_network");
  csma.EnablePcapAll("mixed_network", false);

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll(ascii.CreateFileStream("mixed_network.tr"));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}