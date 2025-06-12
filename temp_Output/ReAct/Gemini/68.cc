#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"
#include "ns3/ping-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeBottleneck");

int main(int argc, char *argv[]) {
  bool enableBql = true;
  std::string queueDiscType = "PfifoFast";
  std::string bandwidth = "10Mbps";
  std::string delay = "1ms";
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
  cmd.AddValue("queueDiscType", "Queue Disc Type (PfifoFast, ARED, CoDel, FqCoDel, PIE)", queueDiscType);
  cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue("delay", "Bottleneck delay", delay);
  cmd.AddValue("simulationTime", "Simulation time", simulationTime);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter(
      "ThreeNodeBottleneck",
      Level::LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2pAccess;
  p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2pAccess.SetChannelAttribute("Delay", StringValue("0.1ms"));

  NetDeviceContainer devices12 = p2pAccess.Install(nodes.Get(0), nodes.Get(1));

  PointToPointHelper p2pBottleneck;
  p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  p2pBottleneck.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices23 = p2pBottleneck.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  TrafficControlHelper tch;
  QueueDiscContainer qdiscs;

  if (enableBql) {
    tch.SetQueueLimits("ns3::DynamicQueueLimits",
                           "MaxPackets", UintegerValue(100));
  }

  if (queueDiscType == "PfifoFast") {
    qdiscs = tch.Install(devices23.Get(1));
  } else if (queueDiscType == "ARED") {
    TypeId queueDiscTypeId = TypeId::LookupByName("ns3::RedQueueDisc");
    tch.SetRootQueueDiscTypeId(queueDiscTypeId);
    qdiscs = tch.Install(devices23.Get(1));
  } else if (queueDiscType == "CoDel") {
    TypeId queueDiscTypeId = TypeId::LookupByName("ns3::CoDelQueueDisc");
    tch.SetRootQueueDiscTypeId(queueDiscTypeId);
    qdiscs = tch.Install(devices23.Get(1));
  } else if (queueDiscType == "FqCoDel") {
    TypeId queueDiscTypeId = TypeId::LookupByName("ns3::FqCoDelQueueDisc");
    tch.SetRootQueueDiscTypeId(queueDiscTypeId);
    qdiscs = tch.Install(devices23.Get(1));
  } else if (queueDiscType == "PIE") {
    TypeId queueDiscTypeId = TypeId::LookupByName("ns3::PieQueueDisc");
    tch.SetRootQueueDiscTypeId(queueDiscTypeId);
    qdiscs = tch.Install(devices23.Get(1));
  } else {
    std::cerr << "Invalid queue disc type." << std::endl;
    return 1;
  }

  uint16_t port = 50000;

  BulkSendHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(interfaces23.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1400));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(simulationTime));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime));

  BulkSendHelper source2("ns3::TcpSocketFactory",
                         InetSocketAddress(interfaces12.GetAddress(0), port + 1));
  source2.SetAttribute("SendSize", UintegerValue(1400));
  ApplicationContainer sourceApps2 = source2.Install(nodes.Get(2));
  sourceApps2.Start(Seconds(1.0));
  sourceApps2.Stop(Seconds(simulationTime));

  PacketSinkHelper sink2("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port + 1));
  ApplicationContainer sinkApps2 = sink2.Install(nodes.Get(0));
  sinkApps2.Start(Seconds(0.0));
  sinkApps2.Stop(Seconds(simulationTime));

  PingHelper ping(interfaces23.GetAddress(1));
  ping.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  ApplicationContainer pingApps = ping.Install(nodes.Get(0));
  pingApps.Start(Seconds(2.0));
  pingApps.Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));

  AsciiTraceHelper ascii;
  p2pBottleneck.EnableAsciiAll(
      "three-node-bottleneck");

  if (enableBql) {
    Config::ConnectWithoutContext(
        "/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/ByteQlim",
        MakeCallback(&QueueSize::TracedCallback));
  }
  Config::ConnectWithoutContext(
        "/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/PacketsInQueue",
        MakeCallback(&QueueSize::TracedCallback));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
           stats.begin();
       i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: "
              << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024
              << " kbps\n";
  }

  monitor->SerializeToXmlFile("three-node-bottleneck-flowmon.xml", true, true);

  Simulator::Destroy();

  return 0;
}