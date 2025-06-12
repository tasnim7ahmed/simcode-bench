#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PersistentTcpRing");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetAttribute("TcpL4Protocol", "RcvBufSize", UintegerValue(65535));

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
  devices[2] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
  devices[3] = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = address.Assign(devices[0]);
  interfaces[1] = address.Assign(devices[1]);
  interfaces[2] = address.Assign(devices[2]);
  interfaces[3] = address.Assign(devices[3]);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  Address serverAddress(InetSocketAddress(interfaces[3].GetAddress(0), port));

  TcpEchoServerHelper echoServerHelper(port);
  ApplicationContainer serverApp = echoServerHelper.Install(nodes.Get(3));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  TcpEchoClientHelper echoClientHelper(interfaces[3].GetAddress(0), port);
  echoClientHelper.SetAttribute("MaxPackets", UintegerValue(0)); // Send unlimited packets
  echoClientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = echoClientHelper.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("persistent-tcp-ring.xml");
  anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0);
  anim.SetConstantPosition(nodes.Get(2), 20.0, 0.0);
  anim.SetConstantPosition(nodes.Get(3), 30.0, 0.0);

  pointToPoint.EnablePcapAll("persistent-tcp-ring", false);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}