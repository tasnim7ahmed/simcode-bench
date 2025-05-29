#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 8080;
  Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
  sourceApp.Start(Seconds(1.0));
  sourceApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("tcp-simple.xml");
  anim.SetConstantPosition(nodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(nodes.Get(1), 2.0, 2.0);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}