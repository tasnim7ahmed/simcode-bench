#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfaces02.GetAddress (0), port));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApp = source.Install (nodes.Get(0));
  sourceApp.Start (Seconds (2.0));
  sourceApp.Stop (Seconds (10.0));

  BulkSendHelper source1 ("ns3::TcpSocketFactory", InetSocketAddress (interfaces12.GetAddress (0), port));
  source1.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApp1 = source1.Install (nodes.Get(1));
  sourceApp1.Start (Seconds (3.0));
  sourceApp1.Stop (Seconds (10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}