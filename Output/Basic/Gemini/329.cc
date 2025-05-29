#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable(true);

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
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  Address serverAddress(InetSocketAddress(interfaces02.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper sourceClient0("ns3::TcpSocketFactory", serverAddress);
  sourceClient0.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer clientApp0 = sourceClient0.Install(nodes.Get(0));
  clientApp0.Start(Seconds(2.0));
  clientApp0.Stop(Seconds(10.0));

  BulkSendHelper sourceClient1("ns3::TcpSocketFactory", serverAddress);
  sourceClient1.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer clientApp1 = sourceClient1.Install(nodes.Get(1));
  clientApp1.Start(Seconds(3.0));
  clientApp1.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}