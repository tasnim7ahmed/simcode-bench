#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

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

  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(interfaces4.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(9.0));

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("ring-animation.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 30, 10);
  anim.SetConstantPosition(nodes.Get(2), 50, 10);
  anim.SetConstantPosition(nodes.Get(3), 70, 10);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }
  monitor->SerializeToXmlFile("ring.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}