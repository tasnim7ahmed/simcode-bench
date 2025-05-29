#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  uint32_t metric_n1_n3 = 20;
  cmd.AddValue("metric_n1_n3", "Metric for link n1-n3", metric_n1_n3);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper pointToPoint_n0_n2;
  pointToPoint_n0_n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint_n0_n2.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper pointToPoint_n1_n2;
  pointToPoint_n1_n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint_n1_n2.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper pointToPoint_n1_n3;
  pointToPoint_n1_n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  pointToPoint_n1_n3.SetChannelAttribute("Delay", StringValue("100ms"));

  PointToPointHelper pointToPoint_n2_n3;
  pointToPoint_n2_n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  pointToPoint_n2_n3.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices_n0_n2 = pointToPoint_n0_n2.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer devices_n1_n2 = pointToPoint_n1_n2.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices_n1_n3 = pointToPoint_n1_n3.Install(nodes.Get(1), nodes.Get(3));
  NetDeviceContainer devices_n2_n3 = pointToPoint_n2_n3.Install(nodes.Get(2), nodes.Get(3));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_n0_n2 = address.Assign(devices_n0_n2);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces_n1_n2 = address.Assign(devices_n1_n2);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces_n1_n3 = address.Assign(devices_n1_n3);
  address.NewNetwork();
  Ipv4InterfaceContainer interfaces_n2_n3 = address.Assign(devices_n2_n3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ipv4GlobalRoutingHelper g;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("routing.routes", std::ios::out);
  g.PrintRoutingTableAllAt(Seconds(5), routingStream);

  // Set the metric for the n1-n3 link
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4StaticRouting> routingTableN1 = staticRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
  routingTableN1->SetLinkMetric(interfaces_n1_n3.GetInterface(0)->GetIp(), metric_n1_n3);

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces_n1_n3.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = echoClient.Install(nodes.Get(3));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Tracing
  pointToPoint_n0_n2.EnableQueueDiscLogging("routing-example-queue-n0-n2", Seconds(0.5));
  pointToPoint_n1_n2.EnableQueueDiscLogging("routing-example-queue-n1-n2", Seconds(0.5));
  pointToPoint_n1_n3.EnableQueueDiscLogging("routing-example-queue-n1-n3", Seconds(0.5));
  pointToPoint_n2_n3.EnableQueueDiscLogging("routing-example-queue-n2-n3", Seconds(0.5));

  devices_n0_n2.Get(0)->TraceConnectWithoutContext("PhyRxDrop", "routing_example.pcap", MakeCallback(&DropTrace));
  devices_n1_n2.Get(0)->TraceConnectWithoutContext("PhyRxDrop", "routing_example.pcap", MakeCallback(&DropTrace));
  devices_n1_n3.Get(0)->TraceConnectWithoutContext("PhyRxDrop", "routing_example.pcap", MakeCallback(&DropTrace));
  devices_n2_n3.Get(0)->TraceConnectWithoutContext("PhyRxDrop", "routing_example.pcap", MakeCallback(&DropTrace));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("routing_example.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}