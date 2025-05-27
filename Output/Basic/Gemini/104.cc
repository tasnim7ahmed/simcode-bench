#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAB, devicesBC;
  devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ipv4StaticRoutingHelper routingHelper;
  Ptr<Ipv4StaticRouting> staticRoutingA = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
  staticRoutingA->AddHostRouteTo(interfacesBC.GetAddress(1), interfacesAB.GetAddress(1), 1);

  Ptr<Ipv4StaticRouting> staticRoutingB = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>()->GetRoutingProtocol());
  staticRoutingB->AddHostRouteTo(interfacesAB.GetAddress(0), Ipv4Address("0.0.0.0"), 1);
  staticRoutingB->AddHostRouteTo(interfacesBC.GetAddress(1), Ipv4Address("0.0.0.0"), 1);

  OnOffHelper onOff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), 9)));
  onOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOff.SetAttribute("PacketSize", UintegerValue(1024));
  onOff.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer onOffApp = onOff.Install(nodes.Get(0));
  onOffApp.Start(Seconds(1.0));
  onOffApp.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("routing", false);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}