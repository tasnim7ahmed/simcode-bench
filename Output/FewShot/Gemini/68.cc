#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ping6-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  std::string queueDiscType = "PfifoFast";
  std::string bottleneckDataRate = "10Mbps";
  std::string bottleneckDelay = "1ms";
  bool enableBql = false;
  std::string simulationTime = "10";

  cmd.AddValue("queueDiscType", "Queue disc type: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueDiscType);
  cmd.AddValue("bottleneckDataRate", "Bottleneck data rate", bottleneckDataRate);
  cmd.AddValue("bottleneckDelay", "Bottleneck delay", bottleneckDelay);
  cmd.AddValue("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  Time simTime = Seconds(std::stod(simulationTime));

  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper accessLink;
  accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  accessLink.SetChannelAttribute("Delay", StringValue("0.1ms"));

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
  bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
  
  NetDeviceContainer devices12 = accessLink.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices23 = bottleneckLink.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  TrafficControlHelper tch;
  QueueDiscTypeId queueDiscTypeId;

  if (queueDiscType == "PfifoFast") {
    queueDiscTypeId = PfifoFastQueueDisc::GetTypeId();
  } else if (queueDiscType == "ARED") {
    queueDiscTypeId = RedQueueDisc::GetTypeId();
  } else if (queueDiscType == "CoDel") {
    queueDiscTypeId = CoDelQueueDisc::GetTypeId();
  } else if (queueDiscType == "FqCoDel") {
    queueDiscTypeId = FqCoDelQueueDisc::GetTypeId();
  } else if (queueDiscType == "PIE") {
    queueDiscTypeId = PieQueueDisc::GetTypeId();
  } else {
    std::cerr << "Invalid queue disc type: " << queueDiscType << std::endl;
    return 1;
  }
  
  AttributeValue queueDiscAttribute = TypeIdValue(queueDiscTypeId);

  tch.SetRootQueueDisc(queueDiscAttribute);
  tch.SetQueueLimits("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));
  
  QueueDiscContainer queueDiscs = tch.Install(devices23.Get(1));
  
  if (enableBql) {
        devices23.Get(1)->SetAttribute("ByteQueueLimits", BooleanValue(true));
  }
  
  uint16_t port = 5000;
  BulkSendHelper source(nodes.Get(0), port);
  source.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps = source.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(simTime - Seconds(1.0));

  PacketSinkHelper sink(port);
  ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(simTime);

  BulkSendHelper source2(nodes.Get(2), port + 1);
  source2.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps2 = source2.Install(nodes.Get(2));
  sourceApps2.Start(Seconds(1.0));
  sourceApps2.Stop(simTime - Seconds(1.0));

  PacketSinkHelper sink2(port + 1);
  ApplicationContainer sinkApps2 = sink2.Install(nodes.Get(0));
  sinkApps2.Start(Seconds(1.0));
  sinkApps2.Stop(simTime);

  
  Ping6Helper ping;
  ping.SetIfIndex(1);
  ping.SetRemote(interfaces23.GetAddress(1));
  ping.SetLocal(interfaces12.GetAddress(0));
  ping.SetCount(10);

  ApplicationContainer pingApp = ping.Install(nodes.Get(0));
  pingApp.Start(Seconds(2.0));
  pingApp.Stop(simTime - Seconds(1.0));
  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(simTime);
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  monitor->SerializeToXmlFile("flowmon.xml", true, true);

  Simulator::Destroy();
  return 0;
}