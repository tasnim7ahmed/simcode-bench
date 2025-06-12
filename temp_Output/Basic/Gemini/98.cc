#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GlobalRoutingMixedNetwork");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_ALL);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer nodes;
  nodes.Create(7);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("30us"));

  NetDeviceContainer p2pDevices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer p2pDevices2 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer csmaDevices = csma.Install(NodeContainer(nodes.Get(1), nodes.Get(2), nodes.Get(3)));
  NetDeviceContainer p2pDevices3 = pointToPoint.Install(nodes.Get(3), nodes.Get(4));
  NetDeviceContainer p2pDevices4 = pointToPoint.Install(nodes.Get(4), nodes.Get(5));
  NetDeviceContainer p2pDevices5 = pointToPoint.Install(nodes.Get(5), nodes.Get(6));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces3 = address.Assign(p2pDevices3);

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces4 = address.Assign(p2pDevices4);

  address.SetBase("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces5 = address.Assign(p2pDevices5);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  Ipv4GlobalRoutingHelper::MarkAsDistanceVectorRouting(nodes.Get(0));
  Ipv4GlobalRoutingHelper::MarkAsDistanceVectorRouting(nodes.Get(6));

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(6));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(20.0));

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(p2pInterfaces5.GetAddress(1), port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer clientApps1 = onoff.Install(nodes.Get(0));
  clientApps1.Start(Seconds(1.0));
  clientApps1.Stop(Seconds(10.0));

  OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(p2pInterfaces5.GetAddress(1), port)));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute("PacketSize", UintegerValue(1024));
  onoff2.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer clientApps2 = onoff2.Install(nodes.Get(0));
  clientApps2.Start(Seconds(11.0));
  clientApps2.Stop(Seconds(20.0));

  Simulator::Schedule(Seconds(5.0), [&]() {
      Ptr<NetDevice> dev = p2pDevices1.Get(0);
      dev->SetAttribute("OperStatus", EnumValue(LINK_DOWN));
      NS_LOG_UNCOND("Interface 0-1 DOWN");
  });

    Simulator::Schedule(Seconds(7.0), [&]() {
      Ptr<NetDevice> dev = p2pDevices1.Get(0);
      dev->SetAttribute("OperStatus", EnumValue(LINK_UP));
      NS_LOG_UNCOND("Interface 0-1 UP");
  });

  Simulator::Schedule(Seconds(13.0), [&]() {
      Ptr<NetDevice> dev = p2pDevices2.Get(0);
      dev->SetAttribute("OperStatus", EnumValue(LINK_DOWN));
      NS_LOG_UNCOND("Interface 0-2 DOWN");
  });

    Simulator::Schedule(Seconds(15.0), [&]() {
      Ptr<NetDevice> dev = p2pDevices2.Get(0);
      dev->SetAttribute("OperStatus", EnumValue(LINK_UP));
      NS_LOG_UNCOND("Interface 0-2 UP");
  });


  csma.EnablePcapAll("global_routing_mixed_network");
  pointToPoint.EnablePcapAll("global_routing_mixed_network");

  AnimationInterface anim("global-routing-mixed-network.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 30, 10);
  anim.SetConstantPosition(nodes.Get(2), 10, 30);
  anim.SetConstantPosition(nodes.Get(3), 30, 30);
  anim.SetConstantPosition(nodes.Get(4), 10, 50);
  anim.SetConstantPosition(nodes.Get(5), 30, 50);
  anim.SetConstantPosition(nodes.Get(6), 10, 70);

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}