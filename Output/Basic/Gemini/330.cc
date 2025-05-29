#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 80;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  HttpClientHelper clientHelper(interfaces.GetAddress(1), port);
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}