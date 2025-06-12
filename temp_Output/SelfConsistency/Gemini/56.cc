#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/stats-module.h"
#include "ns3/tcp-header.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DCTCP");

// Callback function to trace queue state
void
QueueStats(std::string context, uint32_t queueSize)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " " << context << " " << queueSize);
}

int
main(int argc, char* argv[])
{
  // Enable logging
  LogComponentEnable("DCTCP", LOG_LEVEL_INFO);
  LogComponentEnable("TcpWestwood", LOG_LEVEL_ALL); // Enable for Westwood logs
  // Set default values for parameters
  uint32_t nSendersT1T2 = 30;
  uint32_t nSendersT2R1 = 20;
  double simulationTime = 5.0; // Startup + convergence + measurement
  std::string bottleneckT1T2BW = "10Gbps";
  std::string bottleneckT2R1BW = "1Gbps";
  std::string bottleneckT1T2Delay = "20us";
  std::string bottleneckT2R1Delay = "20us";
  std::string queueDiscModel = "ns3::RedQueueDisc";
  uint32_t queueSizeT1T2 = 2000;
  uint32_t queueSizeT2R1 = 2000;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("nSendersT1T2", "Number of senders on T1-T2 link", nSendersT1T2);
  cmd.AddValue("nSendersT2R1", "Number of senders on T2-R1 link", nSendersT2R1);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("bottleneckT1T2BW", "Bottleneck T1-T2 bandwidth", bottleneckT1T2BW);
  cmd.AddValue("bottleneckT2R1BW", "Bottleneck T2-R1 bandwidth", bottleneckT2R1BW);
  cmd.AddValue("bottleneckT1T2Delay", "Bottleneck T1-T2 delay", bottleneckT1T2Delay);
  cmd.AddValue("bottleneckT2R1Delay", "Bottleneck T2-R1 delay", bottleneckT2R1Delay);
  cmd.AddValue("queueDiscModel", "Queue disc model", queueDiscModel);
  cmd.AddValue("queueSizeT1T2", "Queue size for T1-T2 link", queueSizeT1T2);
  cmd.AddValue("queueSizeT2R1", "Queue size for T2-R1 link", queueSizeT2R1);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer sendersT1;
  sendersT1.Create(nSendersT1T2);
  NodeContainer sendersT2;
  sendersT2.Create(nSendersT2R1);
  NodeContainer T1, T2, R1;
  T1.Create(1);
  T2.Create(1);
  R1.Create(1);
  NodeContainer receivers;
  receivers.Create(nSendersT2R1);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(sendersT1);
  stack.Install(sendersT2);
  stack.Install(T1);
  stack.Install(T2);
  stack.Install(R1);
  stack.Install(receivers);

  // Create point-to-point helper for T1-T2 link
  PointToPointHelper p2pT1T2;
  p2pT1T2.SetDeviceAttribute("DataRate", StringValue(bottleneckT1T2BW));
  p2pT1T2.SetChannelAttribute("Delay", StringValue(bottleneckT1T2Delay));
  QueueHelper queueT1T2Helper;
  queueT1T2Helper.SetQueueDiscType(queueDiscModel);
  Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue(bottleneckT1T2BW));
  Config::SetDefault("ns3::RedQueueDisc::LinkDelay", StringValue(bottleneckT1T2Delay));
  Config::SetDefault("ns3::RedQueueDisc::QueueLimit", UintegerValue(queueSizeT1T2));
  p2pT1T2.SetQueue(queueT1T2Helper);

  // Create point-to-point helper for T2-R1 link
  PointToPointHelper p2pT2R1;
  p2pT2R1.SetDeviceAttribute("DataRate", StringValue(bottleneckT2R1BW));
  p2pT2R1.SetChannelAttribute("Delay", StringValue(bottleneckT2R1Delay));
  QueueHelper queueT2R1Helper;
  queueT2R1Helper.SetQueueDiscType(queueDiscModel);
  Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue(bottleneckT2R1BW));
  Config::SetDefault("ns3::RedQueueDisc::LinkDelay", StringValue(bottleneckT2R1Delay));
  Config::SetDefault("ns3::RedQueueDisc::QueueLimit", UintegerValue(queueSizeT2R1));
  p2pT2R1.SetQueue(queueT2R1Helper);

  // Create point-to-point helper for sender-T1 link
  PointToPointHelper p2pSenderT1;
  p2pSenderT1.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2pSenderT1.SetChannelAttribute("Delay", StringValue("20us"));

  // Create point-to-point helper for R1-receiver link
  PointToPointHelper p2pR1Receiver;
  p2pR1Receiver.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2pR1Receiver.SetChannelAttribute("Delay", StringValue("20us"));

  // Install devices
  NetDeviceContainer devicesT1T2 = p2pT1T2.Install(T1, T2);
  NetDeviceContainer devicesT2R1 = p2pT2R1.Install(T2, R1);

  NetDeviceContainer devicesSenderT1[nSendersT1T2];
  for (uint32_t i = 0; i < nSendersT1T2; ++i)
  {
    devicesSenderT1[i] = p2pSenderT1.Install(sendersT1.Get(i), T1);
  }

  NetDeviceContainer devicesSenderT2[nSendersT2R1];
  for (uint32_t i = 0; i < nSendersT2R1; ++i)
  {
    devicesSenderT2[i] = p2pSenderT1.Install(sendersT2.Get(i), T2);
  }

  NetDeviceContainer devicesR1Receiver[nSendersT2R1];
  for (uint32_t i = 0; i < nSendersT2R1; ++i)
  {
    devicesR1Receiver[i] = p2pR1Receiver.Install(R1, receivers.Get(i));
  }

  // Assign addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesT1T2 = address.Assign(devicesT1T2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesT2R1 = address.Assign(devicesT2R1);

  Ipv4InterfaceContainer interfacesSenderT1[nSendersT1T2];
  for (uint32_t i = 0; i < nSendersT1T2; ++i)
  {
    address.SetBase("10.2." + std::to_string(i + 1) + ".0", "255.255.255.0");
    interfacesSenderT1[i] = address.Assign(devicesSenderT1[i]);
  }

    Ipv4InterfaceContainer interfacesSenderT2[nSendersT2R1];
  for (uint32_t i = 0; i < nSendersT2R1; ++i)
  {
    address.SetBase("10.3." + std::to_string(i + 1) + ".0", "255.255.255.0");
    interfacesSenderT2[i] = address.Assign(devicesSenderT2[i]);
  }

  Ipv4InterfaceContainer interfacesR1Receiver[nSendersT2R1];
  for (uint32_t i = 0; i < nSendersT2R1; ++i)
  {
    address.SetBase("10.4." + std::to_string(i + 1) + ".0", "255.255.255.0");
    interfacesR1Receiver[i] = address.Assign(devicesR1Receiver[i]);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create Applications (TCP flows)
  uint16_t port = 50000;
  V4FlowProbe flowProbe;
  ApplicationContainer sinkApps;
  ApplicationContainer sourceApps;

  // Senders from T1 to receivers behind R1
  for (uint32_t i = 0; i < nSendersT1T2; ++i)
  {
        for (uint32_t j = 0; j < nSendersT2R1; ++j) {
            Address sinkLocalAddress(InetSocketAddress{interfacesR1Receiver[j].GetAddress(1), port});
            PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
            sinkApps = packetSinkHelper.Install(receivers.Get(j));
            sinkApps.Start(Seconds(0.0));
            sinkApps.Stop(Seconds(simulationTime));

            // Configure TCP client
            BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkLocalAddress);
            bulkSendHelper.SetAttribute("SendSize", UintegerValue(1448)); // Typical MTU
            sourceApps = bulkSendHelper.Install(sendersT1.Get(i));
            sourceApps.Start(Seconds(0.1 * i)); // Staggered start
            sourceApps.Stop(Seconds(simulationTime));
            flowProbe.AddFlow(sendersT1.Get(i), receivers.Get(j), port);

            ++port;

        }

  }
    // Senders from T2 to receivers behind R1
    for (uint32_t i = 0; i < nSendersT2R1; ++i) {
        Address sinkLocalAddress(InetSocketAddress{interfacesR1Receiver[i].GetAddress(1), port});
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        sinkApps = packetSinkHelper.Install(receivers.Get(i));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simulationTime));

        // Configure TCP client
        BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        bulkSendHelper.SetAttribute("SendSize", UintegerValue(1448)); // Typical MTU
        sourceApps = bulkSendHelper.Install(sendersT2.Get(i));
        sourceApps.Start(Seconds(0.1 * i)); // Staggered start
        sourceApps.Stop(Seconds(simulationTime));
        flowProbe.AddFlow(sendersT2.Get(i), receivers.Get(i), port);

        ++port;


    }


  // Tracing queue size
  AsciiTraceHelper ascii;
  p2pT1T2.EnableAsciiAll(ascii.CreateFileStream("t1t2.tr"));
  p2pT2R1.EnableAsciiAll(ascii.CreateFileStream("t2r1.tr"));

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/TxPackets",
                               MakeCallback(&flowProbe, &V4FlowProbe::FlowMonitor::FlowTx));


  //Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  // Run simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Print flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  double totalThroughput = 0.0;
    uint32_t totalFlows = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    double throughput = (i->second.rxBytes * 8.0) / (simulationTime * 1e6); // Mbps
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress
              << ") Throughput: " << throughput << " Mbps" << std::endl;

        if (i->second.timeLastRxPacket.GetSeconds() > 3.0) { //measure during last 1 second
            totalThroughput += throughput;
            totalFlows++;
        }
  }
    std::cout << "Aggregate Throughput: " << totalThroughput << " Mbps" << std::endl;
    std::cout << "Number of flows used for fairness calculation: " << totalFlows << std::endl;

  // Calculate Jain's Fairness Index
  double sum = 0.0;
  double sumOfSquares = 0.0;
  uint32_t numFlows = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (i->second.timeLastRxPacket.GetSeconds() > 3.0) { //measure during last 1 second
          double throughput = (i->second.rxBytes * 8.0) / (simulationTime * 1e6); // Mbps
          sum += throughput;
          sumOfSquares += throughput * throughput;
          numFlows++;
      }
  }

    double fairnessIndex = 1.0;
    if (numFlows > 0) {
      fairnessIndex = (sum * sum) / (numFlows * sumOfSquares);
    } else {
        fairnessIndex = 0.0;
    }


  std::cout << "Jain's Fairness Index: " << fairnessIndex << std::endl;

  Simulator::Destroy();

  return 0;
}