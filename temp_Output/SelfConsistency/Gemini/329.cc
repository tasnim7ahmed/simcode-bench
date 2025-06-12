#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

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

  uint16_t port = 9;  // well-known echo port number
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Client 0
  BulkSendHelper sourceClient0("ns3::TcpSocketFactory",
                               InetSocketAddress(interfaces02.GetAddress(1), port));
  sourceClient0.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer clientApps0 = sourceClient0.Install(nodes.Get(0));
  clientApps0.Start(Seconds(2.0));
  clientApps0.Stop(Seconds(10.0));

  // Client 1
  BulkSendHelper sourceClient1("ns3::TcpSocketFactory",
                               InetSocketAddress(interfaces12.GetAddress(1), port));
  sourceClient1.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer clientApps1 = sourceClient1.Install(nodes.Get(1));
  clientApps1.Start(Seconds(3.0));
  clientApps1.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}