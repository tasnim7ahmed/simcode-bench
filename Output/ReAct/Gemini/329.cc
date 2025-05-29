#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
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

  Address serverAddress(InetSocketAddress(interfaces02.GetAddress(1), port));
  
  TcpEchoServerHelper echoServerHelper(port);
  ApplicationContainer serverApp = echoServerHelper.Install(nodes.Get(2));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  TcpEchoClientHelper echoClientHelper0(interfaces02.GetAddress(1), port);
  echoClientHelper0.SetAttribute("MaxPackets", UintegerValue(100));
  echoClientHelper0.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClientHelper0.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp0 = echoClientHelper0.Install(nodes.Get(0));
  clientApp0.Start(Seconds(2.0));
  clientApp0.Stop(Seconds(10.0));

  TcpEchoClientHelper echoClientHelper1(interfaces12.GetAddress(1), port);
  echoClientHelper1.SetAttribute("MaxPackets", UintegerValue(100));
  echoClientHelper1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClientHelper1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp1 = echoClientHelper1.Install(nodes.Get(1));
  clientApp1.Start(Seconds(3.0));
  clientApp1.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}