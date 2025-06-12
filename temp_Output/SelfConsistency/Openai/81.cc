#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RedQueueDiscExample");

void
PrintQueueStats(Ptr<QueueDisc> queue, std::string tag)
{
  uint32_t qlen = queue->GetNPackets();
  uint32_t bytes = queue->GetNBytes();
  std::cout << Simulator::Now ().GetSeconds ()
            << "s [" << tag << "] QueueDisc packets: " << qlen
            << ", bytes: " << bytes << std::endl;
}

void
PrintDeviceQueueStats(Ptr<NetDevice> dev, std::string tag)
{
  Ptr<PointToPointNetDevice> ptpDev = dev->GetObject<PointToPointNetDevice>();
  if (ptpDev)
    {
      Ptr<Queue<Packet> > queue = ptpDev->GetQueue();
      if (queue)
        {
          uint32_t qlen = queue->GetNPackets();
          uint32_t bytes = queue->GetNBytes();
          std::cout << Simulator::Now ().GetSeconds ()
                    << "s [" << tag << "] Device queue packets: " << qlen
                    << ", bytes: " << bytes << std::endl;
        }
    }
}

void
PrintSojournTime(Ptr<QueueDisc> queue, std::string tag)
{
  if (queue->GetStats().nSojournTimes)
    {
      double sojourn = queue->GetStats().sojournTimeSum.GetSeconds();
      uint32_t count = queue->GetStats().nSojournTimes;
      double avgSojourn = sojourn / count;
      std::cout << Simulator::Now ().GetSeconds ()
                << "s [" << tag << "] Average sojourn time: " << avgSojourn << " s ("
                << count << " packets)" << std::endl;
    }
}

void
QueueDiscStatsTracer(Ptr<QueueDisc> queue, Ptr<NetDevice> dev, std::string tag)
{
  PrintQueueStats(queue, tag);
  PrintDeviceQueueStats(dev, tag);
  PrintSojournTime(queue, tag);
  Simulator::Schedule(Seconds(0.5), &QueueDiscStatsTracer, queue, dev, tag);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RedQueueDiscExample", LOG_LEVEL_INFO);

  double simTime = 10.0; // seconds

  // 1. Node/container setup
  NodeContainer nodes;
  nodes.Create(2);

  // 2. Point-to-point channel configuration
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue (QueueSize ("100p")));

  NetDeviceContainer devices = p2p.Install(nodes);

  // 3. Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // 4. Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // 5. TrafficControl with RED configuration
  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::RedQueueDisc", "MaxSize", QueueSizeValue (QueueSize ("50p")));
  QueueDiscContainer qdiscs = tch.Install(devices);

  // 6. TCP BulkSend & Sink applications
  uint16_t port = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  BulkSendHelper bulkSend ("ns3::TcpSocketFactory", sinkAddress);
  bulkSend.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
  bulkSend.SetAttribute ("SendSize", UintegerValue (1024));
  ApplicationContainer sourceApp = bulkSend.Install (nodes.Get (0));
  sourceApp.Start (Seconds (0.1));
  sourceApp.Stop (Seconds (simTime));

  // 7. Periodic tracing of queue statistics (queue disc and device queue) & sojourn time
  Simulator::Schedule(Seconds(0.5), &QueueDiscStatsTracer,
                      qdiscs.Get(0),
                      devices.Get(0),
                      "Left Node (Device 0)");

  // 8. FlowMonitor setup
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();

  // 9. Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // 10. Flow Monitor Analysis
  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

  std::cout << "\n=== FlowMonitor Results ===" << std::endl;
  for (const auto& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      double totTime = flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ();
      double throughput = flow.second.rxBytes * 8.0 / (totTime * 1e6); // Mbps
      double meanDelay = (flow.second.delaySum.GetSeconds () / flow.second.rxPackets);
      double meanJitter = (flow.second.jitterSum.GetSeconds () / (flow.second.rxPackets-1));
      std::cout << "Flow ID: " << flow.first << " [" << t.sourceAddress << " --> " << t.destinationAddress << "]\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      std::cout << "  Throughput: " << throughput << " Mbps\n";
      std::cout << "  Mean delay: " << meanDelay*1e3 << " ms\n";
      std::cout << "  Mean jitter: " << meanJitter*1e3 << " ms\n";
    }

  Simulator::Destroy ();

  return 0;
}