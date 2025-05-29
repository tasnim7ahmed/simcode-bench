#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ospf-module.h"
#include "ns3/ipv4-global-routing-helper.h"

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
  NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
  NetDeviceContainer devices5 = pointToPoint.Install(nodes.Get(1), nodes.Get(3));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.SetRoutingHelper(ospf);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces5 = address.Assign(devices5);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces3.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  AnimationInterface anim("ospf-netanim.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 30, 20);
  anim.SetConstantPosition(nodes.Get(2), 50, 30);
  anim.SetConstantPosition(nodes.Get(3), 70, 10);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}