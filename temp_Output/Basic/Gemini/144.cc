#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopology");

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces1.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer2(port);
  ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
  serverApps2.Start(Seconds(3.0));
  serverApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(interfaces2.GetAddress(1), port);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
  clientApps2.Start(Seconds(4.0));
  clientApps2.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer3(port);
  ApplicationContainer serverApps3 = echoServer3.Install(nodes.Get(3));
  serverApps3.Start(Seconds(5.0));
  serverApps3.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient3(interfaces3.GetAddress(1), port);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(2));
  clientApps3.Start(Seconds(6.0));
  clientApps3.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer4(port);
  ApplicationContainer serverApps4 = echoServer4.Install(nodes.Get(0));
  serverApps4.Start(Seconds(7.0));
  serverApps4.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient4(interfaces4.GetAddress(1), port);
  echoClient4.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient4.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps4 = echoClient4.Install(nodes.Get(3));
  clientApps4.Start(Seconds(8.0));
  clientApps4.Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("ring");

  Simulator::Run(Seconds(10.0));
  Simulator::Destroy();
  return 0;
}