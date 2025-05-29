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

  ApplicationContainer serverApps[4];
  serverApps[0] = echoServer.Install(nodes.Get(1));
  serverApps[1] = echoServer.Install(nodes.Get(2));
  serverApps[2] = echoServer.Install(nodes.Get(3));
  serverApps[3] = echoServer.Install(nodes.Get(0));

  serverApps[0].Start(Seconds(0.0));
  serverApps[0].Stop(Seconds(5.0));
  serverApps[1].Start(Seconds(5.0));
  serverApps[1].Stop(Seconds(10.0));
  serverApps[2].Start(Seconds(10.0));
  serverApps[2].Stop(Seconds(15.0));
  serverApps[3].Start(Seconds(15.0));
  serverApps[3].Stop(Seconds(20.0));

  UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[4];
  clientApps[0] = echoClient.Install(nodes.Get(0));
  echoClient.SetRemote(interfaces[1].GetAddress(1),9);
  clientApps[1] = echoClient.Install(nodes.Get(1));
  echoClient.SetRemote(interfaces[2].GetAddress(1),9);
  clientApps[2] = echoClient.Install(nodes.Get(2));
  echoClient.SetRemote(interfaces[3].GetAddress(1),9);
  clientApps[3] = echoClient.Install(nodes.Get(3));
  echoClient.SetRemote(interfaces[0].GetAddress(1),9);

  clientApps[0].Start(Seconds(1.0));
  clientApps[0].Stop(Seconds(5.0));
  clientApps[1].Start(Seconds(6.0));
  clientApps[1].Stop(Seconds(10.0));
  clientApps[2].Start(Seconds(11.0));
  clientApps[2].Stop(Seconds(15.0));
  clientApps[3].Start(Seconds(16.0));
  clientApps[3].Stop(Seconds(20.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}