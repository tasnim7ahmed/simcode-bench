#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(7);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer p2pDevices1, p2pDevices2, p2pDevices3, csmaDevices;
  NodeContainer p2pNodes1, p2pNodes2, p2pNodes3, csmaNodes;

  p2pNodes1.Add(nodes.Get(0));
  p2pNodes1.Add(nodes.Get(3));
  p2pDevices1 = pointToPoint.Install(p2pNodes1);

  p2pNodes2.Add(nodes.Get(1));
  p2pNodes2.Add(nodes.Get(3));
  p2pDevices2 = pointToPoint.Install(p2pNodes2);

  p2pNodes3.Add(nodes.Get(2));
  p2pNodes3.Add(nodes.Get(3));
  p2pDevices3 = pointToPoint.Install(p2pNodes3);

  csmaNodes.Add(nodes.Get(3));
  csmaNodes.Add(nodes.Get(4));
  csmaNodes.Add(nodes.Get(5));
  csmaNodes.Add(nodes.Get(6));
  csmaDevices = csma.Install(csmaNodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces3 = address.Assign(p2pDevices3);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  Ipv4GlobalRoutingHelper::NotifyInterfaceUp(nodes.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetInterface());
  Ipv4GlobalRoutingHelper::NotifyInterfaceUp(nodes.Get(1)->GetObject<Ipv4>()->GetAddress(1,0).GetInterface());
  Ipv4GlobalRoutingHelper::NotifyInterfaceUp(nodes.Get(2)->GetObject<Ipv4>()->GetAddress(1,0).GetInterface());
  Ipv4GlobalRoutingHelper::NotifyInterfaceUp(nodes.Get(4)->GetObject<Ipv4>()->GetAddress(1,0).GetInterface());
  Ipv4GlobalRoutingHelper::NotifyInterfaceUp(nodes.Get(5)->GetObject<Ipv4>()->GetAddress(1,0).GetInterface());
  Ipv4GlobalRoutingHelper::NotifyInterfaceUp(nodes.Get(6)->GetObject<Ipv4>()->GetAddress(1,0).GetInterface());

  uint16_t port = 9;

  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(csmaInterfaces.GetAddress(3), port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer apps1 = onoff.Install(nodes.Get(0));
  apps1.Start(Seconds(1.0));
  apps1.Stop(Seconds(20.0));

  OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(csmaInterfaces.GetAddress(3), port)));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute("PacketSize", UintegerValue(1024));
  onoff2.SetAttribute("DataRate", StringValue("1Mbps"));

  ApplicationContainer apps2 = onoff2.Install(nodes.Get(1));
  apps2.Start(Seconds(11.0));
  apps2.Stop(Seconds(20.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(6));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(20.0));

  Simulator::Schedule(Seconds(5.0), [&]() {
      p2pDevices1.Get(0)->GetObject<PointToPointNetDevice>()->SetIfDown();
      std::cout << "Interface 10.1.1.1 Down" << std::endl;
  });

  Simulator::Schedule(Seconds(15.0), [&]() {
      p2pDevices1.Get(0)->GetObject<PointToPointNetDevice>()->SetIfUp();
      std::cout << "Interface 10.1.1.1 Up" << std::endl;
  });

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll(ascii.CreateFileStream("routing.tr"));

  Simulator::Schedule(Seconds(0.00001), [&]() {
    csma.EnablePcapAll("csma", false);
    pointToPoint.EnablePcapAll("point-to-point", false);
  });

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  monitor->SerializeToXmlFile("routing.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}