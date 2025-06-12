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
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps0 = echoServer.Install(nodes.Get(0));
  serverApps0.Start(Seconds(0.0));
  serverApps0.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient0(interfaces1.GetAddress(1), 9);
  echoClient0.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps0 = echoClient0.Install(nodes.Get(1));
  clientApps0.Start(Seconds(1.0));
  clientApps0.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer1(9);
   ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(2));
  serverApps1.Start(Seconds(11.0));
  serverApps1.Stop(Seconds(20.0));
  
  UdpEchoClientHelper echoClient1(interfaces2.GetAddress(1), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
  clientApps1.Start(Seconds(12.0));
  clientApps1.Stop(Seconds(20.0));
  
  UdpEchoServerHelper echoServer2(9);

  ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(3));
  serverApps2.Start(Seconds(21.0));
  serverApps2.Stop(Seconds(30.0));

  UdpEchoClientHelper echoClient2(interfaces3.GetAddress(1), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
  clientApps2.Start(Seconds(22.0));
  clientApps2.Stop(Seconds(30.0));

  UdpEchoServerHelper echoServer3(9);

  ApplicationContainer serverApps3 = echoServer3.Install(nodes.Get(1));
  serverApps3.Start(Seconds(31.0));
  serverApps3.Stop(Seconds(40.0));

  UdpEchoClientHelper echoClient3(interfaces1.GetAddress(0), 9);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(0));
  clientApps3.Start(Seconds(32.0));
  clientApps3.Stop(Seconds(40.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  AnimationInterface anim("ring_network.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 30, 10);
  anim.SetConstantPosition(nodes.Get(2), 50, 10);
  anim.SetConstantPosition(nodes.Get(3), 70, 10);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}