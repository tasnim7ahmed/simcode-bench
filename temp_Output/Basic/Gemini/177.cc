#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  bottleneckLink.SetChannelAttribute("Delay", StringValue("20ms"));
  
  PointToPointHelper leftLink;
  leftLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  leftLink.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper rightLink;
  rightLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  rightLink.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer bottleneckDevices;
  bottleneckDevices = bottleneckLink.Install(nodes.Get(1), nodes.Get(2));

  NetDeviceContainer leftDevices;
  leftDevices = leftLink.Install(nodes.Get(0), nodes.Get(1));

  NetDeviceContainer rightDevices;
  rightDevices = rightLink.Install(nodes.Get(2), nodes.Get(0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftInterfaces = address.Assign(leftDevices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer rightInterfaces = address.Assign(rightDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 5000;
  BulkSendHelper source1("ns3::TcpSocketFactory",
                           InetSocketAddress(rightInterfaces.GetAddress(0), port));
  source1.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps1 = source1.Install(nodes.Get(0));
  sourceApps1.Start(Seconds(1.0));
  sourceApps1.Stop(Seconds(10.0));

  BulkSendHelper source2("ns3::TcpSocketFactory",
                           InetSocketAddress(rightInterfaces.GetAddress(0), port+1));
  source2.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps2 = source2.Install(nodes.Get(1));
  sourceApps2.Start(Seconds(1.0));
  sourceApps2.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(11.0));

  PacketSinkHelper sink2("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port+1));
  ApplicationContainer sinkApps2 = sink2.Install(nodes.Get(0));
  sinkApps2.Start(Seconds(0.0));
  sinkApps2.Stop(Seconds(11.0));

  // Change TCP Congestion Control Algorithm
  std::string tcpVariant = "ns3::TcpReno";
  GlobalValue::Bind("TcpL4Protocol::SocketType", StringValue(tcpVariant));

  // Animation
  AnimationInterface anim("congestion_control.xml");
  anim.SetConstantPosition(nodes.Get(0), 1, 2);
  anim.SetConstantPosition(nodes.Get(1), 5, 2);
  anim.SetConstantPosition(nodes.Get(2), 9, 2);

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}