#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpSimulation");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_ERROR);
  LogComponent::SetLogLevel(
    "DctcpSimulation", (LogLevel) (LOG_LEVEL_INFO | LOG_PREFIX_TIME | LOG_PREFIX_NODE));

  // Create nodes
  NodeContainer senders;
  senders.Create(40);
  NodeContainer T1, T2, R1;
  T1.Create(1);
  T2.Create(1);
  R1.Create(1);
  NodeContainer receivers;
  receivers.Create(20);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(senders);
  stack.Install(T1);
  stack.Install(T2);
  stack.Install(R1);
  stack.Install(receivers);

  // PointToPointHelper for links
  PointToPointHelper pointToPointT1T2;
  pointToPointT1T2.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  pointToPointT1T2.SetChannelAttribute("Delay", StringValue("10us"));

  PointToPointHelper pointToPointT2R1;
  pointToPointT2R1.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  pointToPointT2R1.SetChannelAttribute("Delay", StringValue("10us"));

  PointToPointHelper pointToPointR1Receivers;
  pointToPointR1Receivers.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  pointToPointR1Receivers.SetChannelAttribute("Delay", StringValue("10us"));

  PointToPointHelper pointToPointSendersT1;
  pointToPointSendersT1.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  pointToPointSendersT1.SetChannelAttribute("Delay", StringValue("10us"));

  // Install NetDevices
  NetDeviceContainer sendersT1Devices;
  for (uint32_t i = 0; i < 30; ++i) {
    NetDeviceContainer devices = pointToPointSendersT1.Install(senders.Get(i), T1.Get(0));
    sendersT1Devices.Add(devices.Get(0));
  }

  NetDeviceContainer T1T2Devices = pointToPointT1T2.Install(T1.Get(0), T2.Get(0));

  NetDeviceContainer T2R1Devices = pointToPointT2R1.Install(T2.Get(0), R1.Get(0));

  NetDeviceContainer R1ReceiversDevices;
  for (uint32_t i = 0; i < 20; ++i) {
    NetDeviceContainer devices = pointToPointR1Receivers.Install(R1.Get(0), receivers.Get(i));
    R1ReceiversDevices.Add(devices.Get(1));
  }

  for (uint32_t i = 30; i < 40; ++i) {
    NetDeviceContainer devices = pointToPointSendersT1.Install(senders.Get(i), T2.Get(0));
    sendersT1Devices.Add(devices.Get(0));
  }

  // Install QueueDisc on bottleneck links using RED with ECN
  TrafficControlHelper tch_t1t2, tch_t2r1;
  QueueDiscTypeId queueid = TypeId::LookupByName("ns3::RedQueueDisc");
  tch_t1t2.SetRootQueueDisc(
    "ns3::RedQueueDisc", "LinkBandwidth", StringValue("10Gbps"), "LinkDelay", StringValue("10us"));
  tch_t2r1.SetRootQueueDisc(
    "ns3::RedQueueDisc", "LinkBandwidth", StringValue("1Gbps"), "LinkDelay", StringValue("10us"));

  NetDeviceContainer tempContainer;
  tempContainer.Add(T1T2Devices.Get(0));
  tch_t1t2.Install(tempContainer);

  tempContainer.Clear();
  tempContainer.Add(T2R1Devices.Get(0));
  tch_t2r1.Install(tempContainer);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sendersT1Interfaces;
  for (uint32_t i = 0; i < 30; ++i) {
    NetDeviceContainer devices;
    devices.Add(sendersT1Devices.Get(i));
    devices.Add(T1T2Devices.Get(0)->GetNode()->GetDevice(0));
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    sendersT1Interfaces.Add(interfaces.Get(0));
    address.NewNetwork();
  }

  Ipv4InterfaceContainer T1T2Interfaces = address.Assign(T1T2Devices);
  address.NewNetwork();
  Ipv4InterfaceContainer T2R1Interfaces = address.Assign(T2R1Devices);
  address.NewNetwork();
  Ipv4InterfaceContainer R1ReceiversInterfaces;

  for (uint32_t i = 0; i < 20; ++i) {
    NetDeviceContainer devices;
    devices.Add(R1ReceiversDevices.Get(i));
    devices.Add(T2R1Devices.Get(1)->GetNode()->GetDevice(0));
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    R1ReceiversInterfaces.Add(interfaces.Get(1));
    address.NewNetwork();
  }
   for (uint32_t i = 30; i < 40; ++i) {
      NetDeviceContainer devices;
      devices.Add(sendersT1Devices.Get(i));
      devices.Add(T1T2Devices.Get(1)->GetNode()->GetDevice(0));
      Ipv4InterfaceContainer interfaces = address.Assign(devices);
      sendersT1Interfaces.Add(interfaces.Get(0));
      address.NewNetwork();

  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create TCP applications
  uint16_t port = 50000;
  ApplicationContainer sinkApps;
  ApplicationContainer clientApps;

  for (uint32_t i = 0; i < 20; ++i) {
    BulkSendHelper source("ns3::TcpSocketFactory",
                          InetSocketAddress(R1ReceiversInterfaces.Get(i).GetAddress(0), port));
    source.SetAttribute("SendSize", UintegerValue(1460));
    ApplicationContainer client = source.Install(senders.Get(i * 2));
    clientApps.Add(client);
    client[0]->SetStartTime(Seconds(0.1 * i));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer server = sink.Install(receivers.Get(i));
    sinkApps.Add(server);
    server.Start(Seconds(0.0));
    port++;

        BulkSendHelper source2("ns3::TcpSocketFactory",
                          InetSocketAddress(R1ReceiversInterfaces.Get(i).GetAddress(0), port));
    source2.SetAttribute("SendSize", UintegerValue(1460));
    ApplicationContainer client2 = source2.Install(senders.Get(i * 2 + 1));
    clientApps.Add(client2);
    client2[0]->SetStartTime(Seconds(0.1 * i));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer server2 = sink2.Install(receivers.Get(i));
    sinkApps.Add(server2);
    server2.Start(Seconds(0.0));
    port++;

  }

  clientApps.Start(Seconds(3.0));
  clientApps.Stop(Seconds(4.0));
  sinkApps.Stop(Seconds(5.0));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(5.0));
  Simulator::Run();

  // Analyze results
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  double totalThroughput = 0.0;
  std::vector<double> flowThroughputs;

  std::ofstream throughputFile("throughput.dat");
  throughputFile << "FlowId Throughput (Mbps)" << std::endl;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end();
       ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() -
                                                      i->second.timeFirstTxPacket.GetSeconds()) /
                        1000000;
    totalThroughput += throughput;
    flowThroughputs.push_back(throughput);

    throughputFile << i->first << " " << throughput << std::endl;

    NS_LOG_INFO("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress
                         << ") : Throughput: " << throughput << " Mbps");
  }
  throughputFile.close();

  // Calculate aggregate fairness (Jain's fairness index)
  double sum = 0.0;
  double sumOfSquares = 0.0;
  for (double throughput : flowThroughputs) {
    sum += throughput;
    sumOfSquares += throughput * throughput;
  }

  double fairnessIndex = (sum * sum) / (flowThroughputs.size() * sumOfSquares);
  NS_LOG_INFO("Aggregate Fairness Index: " << fairnessIndex);
  std::cout << "Aggregate Fairness Index: " << fairnessIndex << std::endl;

  // Output queue statistics (T1T2 link)
  Ptr<QueueDisc> queue =
    StaticCast<PointToPointNetDevice>(T1T2Devices.Get(0))->GetQueue()->GetQueueDisc();
  if (queue != nullptr) {
    std::cout << "T1T2 Queue Statistics:" << std::endl;
    std::cout << "  Avg Queue Size: " << queue->GetAverageQueueSize().GetValue() << std::endl;
    std::cout << "  Drop Count: " << queue->GetDropCount() << std::endl;
    std::cout << "  Mark Count: " << queue->GetMarkCount() << std::endl;
  }

  // Output queue statistics (T2R1 link)
  queue = StaticCast<PointToPointNetDevice>(T2R1Devices.Get(0))->GetQueue()->GetQueueDisc();
  if (queue != nullptr) {
    std::cout << "T2R1 Queue Statistics:" << std::endl;
    std::cout << "  Avg Queue Size: " << queue->GetAverageQueueSize().GetValue() << std::endl;
    std::cout << "  Drop Count: " << queue->GetDropCount() << std::endl;
    std::cout << "  Mark Count: " << queue->GetMarkCount() << std::endl;
  }

  monitor->SerializeToXmlFile("dctcp_simulation.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}