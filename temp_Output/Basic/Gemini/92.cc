#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer srcNode, destNode, rtr1Node, rtr2Node, destRtrNode;
  srcNode.Create(1);
  destNode.Create(1);
  rtr1Node.Create(1);
  rtr2Node.Create(1);
  destRtrNode.Create(1);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer srcRtr1Devs = p2p.Install(srcNode.Get(0), rtr1Node.Get(0));
  NetDeviceContainer srcRtr2Devs = p2p.Install(srcNode.Get(0), rtr2Node.Get(0));
  NetDeviceContainer rtr1DestRtrDevs = p2p.Install(rtr1Node.Get(0), destRtrNode.Get(0));
  NetDeviceContainer rtr2DestRtrDevs = p2p.Install(rtr2Node.Get(0), destRtrNode.Get(0));
  NetDeviceContainer destRtrDestDevs = p2p.Install(destRtrNode.Get(0), destNode.Get(0));

  InternetStackHelper stack;
  stack.Install(srcNode);
  stack.Install(destNode);
  stack.Install(rtr1Node);
  stack.Install(rtr2Node);
  stack.Install(destRtrNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer srcRtr1Ifaces = address.Assign(srcRtr1Devs);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer srcRtr2Ifaces = address.Assign(srcRtr2Devs);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer rtr1DestRtrIfaces = address.Assign(rtr1DestRtrDevs);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer rtr2DestRtrIfaces = address.Assign(rtr2DestRtrDevs);

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer destRtrDestIfaces = address.Assign(destRtrDestDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Static routing configuration
  Ptr<Ipv4StaticRouting> staticRoutingRtr1 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (rtr1Node.Get(0)->GetObject<Ipv4>());
  staticRoutingRtr1->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), rtr1DestRtrIfaces.GetAddress(1), 1);

  Ptr<Ipv4StaticRouting> staticRoutingRtr2 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (rtr2Node.Get(0)->GetObject<Ipv4>());
  staticRoutingRtr2->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), rtr2DestRtrIfaces.GetAddress(1), 1);

  Ptr<Ipv4StaticRouting> staticRoutingDestRtr = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (destRtrNode.Get(0)->GetObject<Ipv4>());
  staticRoutingDestRtr->AddHostRouteTo(srcRtr1Ifaces.GetAddress(0), rtr1DestRtrIfaces.GetAddress(0), 1);
  staticRoutingDestRtr->AddHostRouteTo(srcRtr2Ifaces.GetAddress(0), rtr2DestRtrIfaces.GetAddress(0), 1);

  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(destRtrDestIfaces.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1024));
  source.SetAttribute("MaxBytes", UintegerValue(1000000));
  ApplicationContainer sourceApp = source.Install(srcNode.Get(0));
  sourceApp.Start(Seconds(1.0));
  sourceApp.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(destNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}