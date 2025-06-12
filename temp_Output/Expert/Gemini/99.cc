#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ptr<Node> routerB = nodes.Get(1);
  Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetStaticRouting(routerB->GetObject<Ipv4>());
  staticRoutingB->AddHostRouteToNetwork(Ipv4Address("10.1.1.1"), Ipv4Address("255.255.255.255"), interfacesAB.GetAddress(0), 1);
  staticRoutingB->AddHostRouteToNetwork(Ipv4Address("10.1.2.3"), Ipv4Address("255.255.255.255"), interfacesBC.GetAddress(1), 1);

  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), 9)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(interfacesBC.GetAddress(1), 9));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  p2p.EnablePcapAll("pcap-trace");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
  }

  monitor->SerializeToXmlFile("flowmon.xml", true, true);

  Simulator::Destroy();
  return 0;
}