#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  devices[2] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
  devices[3] = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = address.Assign(devices[0]);
  interfaces[1] = address.Assign(devices[1]);
  interfaces[2] = address.Assign(devices[2]);
  interfaces[3] = address.Assign(devices[3]);

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(5.0));

  UdpEchoServerHelper echoServer2(9);

  ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(3));
  serverApps2.Start(Seconds(5.0));
  serverApps2.Stop(Seconds(15.0));

  UdpEchoClientHelper echoClient2(interfaces[2].GetAddress(1), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
  clientApps2.Start(Seconds(6.0));
  clientApps2.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run(Seconds(10.0));
  Simulator::Destroy();
  return 0;
}