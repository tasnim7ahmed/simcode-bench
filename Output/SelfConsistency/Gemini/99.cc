#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterNetwork");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetAttribute("UdpClient", "Interval", StringValue("0.01"));

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Add static routes for A and C
  Ptr<Ipv4StaticRouting> staticRoutingA = Ipv4RoutingHelper::GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
  staticRoutingA->AddHostRouteToNetwork(Ipv4Address("10.1.2.2"), Ipv4Address("0.0.0.0"), 1);
  staticRoutingA->SetDefaultRoute(interfacesAB.GetAddress(1), 1);

  Ptr<Ipv4StaticRouting> staticRoutingC = Ipv4RoutingHelper::GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
  staticRoutingC->AddHostRouteToNetwork(Ipv4Address("10.1.1.1"), Ipv4Address("0.0.0.0"), 1);
  staticRoutingC->SetDefaultRoute(interfacesBC.GetAddress(1), 1);
  
  // Add routes at node B to reach the other subnets
  Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
  staticRoutingB->AddHostRouteToNetwork(Ipv4Address("10.1.1.1"), Ipv4Address("0.0.0.0"), 1);
  staticRoutingB->AddHostRouteToNetwork(Ipv4Address("10.1.2.2"), Ipv4Address("0.0.0.0"), 1);

  uint16_t port = 9;  // Discard port (RFC 863)

  OnOffHelper onoff("ns3::UdpClient",
                    Address(InetSocketAddress(interfacesBC.GetAddress(1), port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpServer",
                         Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("three-router");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}