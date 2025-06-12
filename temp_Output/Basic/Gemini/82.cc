#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool useIpv6 = false;

  CommandLine cmd;
  cmd.AddValue("useIpv6", "Set to true to use IPv6 instead of IPv4", useIpv6);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  QueueSize queueSize = QueueSize("1000p");
  pointToPoint.SetQueue("ns3::DropTailQueue", "MaxPackets", StringValue("1000"));

  NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices13 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices14 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces13 = address.Assign(devices13);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces14 = address.Assign(devices14);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces12.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll("udp-echo.tr");

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}