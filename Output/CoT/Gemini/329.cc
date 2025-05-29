#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

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

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient0(interfaces02.GetAddress(1), 9);
  echoClient0.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient0.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient0.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps0 = echoClient0.Install(nodes.Get(0));
  clientApps0.Start(Seconds(2.0));
  clientApps0.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient1(interfaces12.GetAddress(1), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient1.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
  clientApps1.Start(Seconds(3.0));
  clientApps1.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}