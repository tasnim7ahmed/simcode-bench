#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  std::string queueDiscType = "ns3::FifoQueueDisc";
  uint32_t queueSize = 1000;
  std::string rate = "5Mbps";
  std::string delay = "2ms";
  std::string altRate = "1.5Mbps";
  std::string altDelay = "10ms";
  std::string n1n3Rate = "1.5Mbps";
  std::string n1n3Delay = "100ms";
  uint32_t packetSize = 1024;
  std::string appDataRate = "1Mbps";
  double appStartTime = 1.0;
  double appStopTime = 10.0;
  uint32_t metric = 1;

  cmd.AddValue("queueDiscType", "QueueDiscType: FifoQueueDisc, PfifoFastQueueDisc, RedQueueDisc", queueDiscType);
  cmd.AddValue("queueSize", "Max queue size", queueSize);
  cmd.AddValue("rate", "P2P link data rate", rate);
  cmd.AddValue("delay", "P2P link delay", delay);
  cmd.AddValue("altRate", "Alternate link data rate", altRate);
  cmd.AddValue("altDelay", "Alternate link delay", altDelay);
  cmd.AddValue("n1n3Rate", "n1n3 link data rate", n1n3Rate);
  cmd.AddValue("n1n3Delay", "n1n3 link delay", n1n3Delay);
  cmd.AddValue("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue("appDataRate", "Rate at which packets are sent", appDataRate);
  cmd.AddValue("appStartTime", "Application start time", appStartTime);
  cmd.AddValue("appStopTime", "Application stop time", appStopTime);
  cmd.AddValue("metric", "Routing Metric for n1n3 link", metric);

  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(rate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc(queueDiscType, "MaxSize", StringValue(std::to_string(queueSize) + "p"));

  NetDeviceContainer d02 = p2p.Install(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));

  PointToPointHelper p2pAlt;
  p2pAlt.SetDeviceAttribute("DataRate", StringValue(altRate));
  p2pAlt.SetChannelAttribute("Delay", StringValue(altDelay));
  NetDeviceContainer d23 = p2pAlt.Install(nodes.Get(2), nodes.Get(3));

  PointToPointHelper p2pN1N3;
  p2pN1N3.SetDeviceAttribute("DataRate", StringValue(n1n3Rate));
  p2pN1N3.SetChannelAttribute("Delay", StringValue(n1n3Delay));
  NetDeviceContainer d13 = p2pN1N3.Install(nodes.Get(1), nodes.Get(3));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign(d02);
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign(d12);
  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign(d23);
  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i13 = ipv4.Assign(d13);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ipv4 l3 = nodes.Get(1)->GetObject<Ipv4>();
  Ptr<Ipv4RoutingTable> rt = l3.GetRoutingProtocol()->GetObject<Ipv4RoutingTable>();
  rt->SetMetric(i13.GetAddress(0), i13.GetAddress(1), metric);

  uint16_t port = 9;
  UdpClientHelper client(i12.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("1ms")));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = client.Install(nodes.Get(3));
  clientApps.Start(Seconds(appStartTime));
  clientApps.Stop(Seconds(appStopTime));

  UdpServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(appStartTime));
  serverApps.Stop(Seconds(appStopTime));

  Simulator::Schedule(Seconds(appStartTime + 0.1), [](){
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback(&DropBeforeEnqueue::Enqueue));
  });

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll(ascii.CreateFileStream("routing.tr"));
  p2pAlt.EnableAsciiAll(ascii.CreateFileStream("routing.tr"));
  p2pN1N3.EnableAsciiAll(ascii.CreateFileStream("routing.tr"));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(appStopTime + 1));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == i12.GetAddress(0)) {
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }
  }

  Simulator::Destroy();
  return 0;
}