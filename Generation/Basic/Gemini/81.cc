#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/red-queue-disc.h" // Required for RedQueueDisc specific parameters

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RedQueueDiscSimulation");

/**
 * \brief Callback for TrafficControlLayer's queue disc length changes.
 *
 * This function is connected to the TxQueueDiscLen trace source of a
 * TrafficControlLayer instance. It prints the old and new queue disc lengths.
 *
 * \param context The context string for the trace source.
 * \param oldLen The queue disc length before the change.
 * \param newLen The queue disc length after the change.
 */
void
QueueDiscLengthTrace (std::string context, uint32_t oldLen, uint32_t newLen)
{
  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                         << "s: " << context << " TrafficControlLayer (QueueDisc) queue length changed from "
                         << oldLen << " to " << newLen);
}

/**
 * \brief Callback for a NetDevice's internal (e.g., TxQueue) queue length changes.
 *
 * This function is connected to the PacketsInQueue trace source of a Queue
 * instance (e.g., the TxQueue of a PointToPointNetDevice). It prints the old
 * and new device queue lengths.
 *
 * \param context The context string for the trace source.
 * \param oldLen The device queue length before the change.
 * \param newLen The device queue length after the change.
 */
void
DeviceQueueLengthTrace (std::string context, uint32_t oldLen, uint32_t newLen)
{
  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                         << "s: " << context << " NetDevice (internal) queue length changed from "
                         << oldLen << " to " << newLen);
}

int
main (int argc, char *argv[])
{
  // 1. Set up simulation parameters
  double simulationTime = 10.0; // seconds
  std::string dataRate = "10Mbps";
  std::string channelDelay = "2ms";
  uint32_t redMinThreshold = 5;      // packets
  uint32_t redMaxThreshold = 15;     // packets
  double redQueueWeight = 0.002;
  uint32_t redQueueLimit = 20;       // packets (overall queue disc limit)

  // Configure command line arguments for flexibility
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simulationTime", "Total duration of the simulation in seconds", simulationTime);
  cmd.AddValue ("dataRate", "Data rate of the point-to-point link", dataRate);
  cmd.AddValue ("channelDelay", "Delay of the point-to-point channel", channelDelay);
  cmd.AddValue ("redMinThreshold", "RED queue disc minimum threshold in packets", redMinThreshold);
  cmd.AddValue ("redMaxThreshold", "RED queue disc maximum threshold in packets", redMaxThreshold);
  cmd.AddValue ("redQueueWeight", "RED queue disc weight (qW)", redQueueWeight);
  cmd.AddValue ("redQueueLimit", "RED queue disc overall queue limit in packets", redQueueLimit);
  cmd.Parse (argc, argv);

  // Set default TCP variant to NewReno and segment size
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448)); // Typical MSS for Ethernet

  // Enable logging for relevant components
  LogComponentEnable ("RedQueueDiscSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("TrafficControlHelper", LOG_LEVEL_INFO);
  LogComponentEnable ("RedQueueDisc", LOG_LEVEL_INFO);
  LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  LogComponentEnable ("FlowMonitor", LOG_LEVEL_INFO);

  // 2. Create nodes and point-to-point link
  NS_LOG_INFO ("Creating two nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  NS_LOG_INFO ("Creating point-to-point link with DataRate: " << dataRate << " and Delay: " << channelDelay);
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (channelDelay));
  NetDeviceContainer devices;
  devices = p2p.Install (nodes);

  // 3. Install internet stack and assign IP addresses
  NS_LOG_INFO ("Installing internet stack and assigning IP addresses.");
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 4. Configure and install RedQueueDisc on Node 0's device (outgoing interface)
  NS_LOG_INFO ("Installing RedQueueDisc on Node 0's egress device (" << devices.Get (0)->GetAddress () << ").");
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                        "MinTh", DoubleValue (redMinThreshold),
                        "MaxTh", DoubleValue (redMaxThreshold),
                        "QW", DoubleValue (redQueueWeight),
                        "Limit", UintegerValue (redQueueLimit));
  
  // Install the RedQueueDisc on the first device (Node 0's egress interface)
  tch.Install (devices.Get (0));

  // 5. Track and print statistics about queue lengths
  // Connect to the TxQueueDiscLen trace source for traffic control layer queue length
  // This trace source is available on the TrafficControlLayer object itself.
  // The path refers to Node 0's TrafficControlLayer.
  Config::Connect ("/NodeList/0/$ns3::TrafficControlLayer/TxQueueDiscLen", MakeCallback (&QueueDiscLengthTrace));
  NS_LOG_INFO ("Connected to TrafficControlLayer TxQueueDiscLen trace source for Node 0.");

  // Connect to the PacketsInQueue trace source of the PointToPointNetDevice's internal TxQueue.
  // This represents the queue at the device layer before the TrafficControlLayer.
  // The path refers to Node 0's first device (index 0) and its internal TxQueue.
  Config::Connect ("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/PacketsInQueue", MakeCallback (&DeviceQueueLengthTrace));
  NS_LOG_INFO ("Connected to PointToPointNetDevice TxQueue PacketsInQueue trace source for Node 0.");
  
  // Sojourn time of packets is reported as end-to-end delay by FlowMonitor.

  // 6. Configure TCP applications (BulkSend and PacketSink)
  NS_LOG_INFO ("Setting up TCP BulkSend (Node 0) and PacketSink (Node 1) applications.");
  uint16_t port = 9; // Echo port

  // PacketSink application on Node 1 (receiver)
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));

  // BulkSend application on Node 0 (sender)
  BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", sinkAddress);
  // Send indefinitely (until stopped by simulation time)
  sourceHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  sourceHelper.SetAttribute ("SendSize", UintegerValue (1448)); // Send full segments
  ApplicationContainer sourceApps = sourceHelper.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0)); // Start sending after 1 second
  sourceApps.Stop (Seconds (simulationTime - 0.5)); // Stop before the sink application

  // 7. Configure FlowMonitor for flow statistics
  NS_LOG_INFO ("Configuring FlowMonitor.");
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  // 8. Run simulation
  NS_LOG_INFO ("Running simulation for " << simulationTime << " seconds...");
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // 9. Report FlowMonitor statistics
  NS_LOG_INFO ("\n--- FlowMonitor Statistics Report ---");
  // Check for any lost packets within the monitor
  monitor->CheckFor=          ">"          "LostPackets ();";
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.Get= "FlowClassifier ());";
  FlowMonitor::FlowStatsContainer stats = monitor->Get= "FlowStats ();";

  double totalThroughputMbps = 0;
  uint32_t totalRxPackets = 0;
  Time totalDelaySum = Seconds (0);
  Time totalJitterSum = Seconds (0);
  uint32_t activeFlowCount = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    
    // Only report for TCP flows from Node 0 (sender) to Node 1 (receiver)
    if (t.sourceAddress == interfaces.GetAddress (0) &&
        t.destinationAddress == interfaces.GetAddress (1) &&
        t.protocol == 6 /* TCP */)
    {
      NS_LOG_INFO ("  Flow ID: " << i->first << " (Source: " << t.sourceAddress << ", Dest: " << t.destinationAddress << ")");
      NS_LOG_INFO ("    Tx Bytes: " << i->second.txBytes);
      NS_LOG_INFO ("    Rx Bytes: " << i->second.rxBytes);
      NS_LOG_INFO ("    Tx Packets: " << i->second.txPackets);
      NS_LOG_INFO ("    Rx Packets: " << i->second.rxPackets);
      NS_LOG_INFO ("    Lost Packets: " << i->second.lostPackets);
      
      if (i->second.rxPackets > 0)
      {
        double flowDuration = i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ();
        double throughputMbps = (i->second.rxBytes * 8.0) / (flowDuration * 1000000.0);
        NS_LOG_INFO ("    Throughput: " << throughputMbps << " Mbps");
        
        double meanDelayMs = (i->second.delaySum.GetSeconds () / i->second.rxPackets) * 1000;
        NS_LOG_INFO ("    Mean Delay: " << meanDelayMs << " ms"); // Represents end-to-end sojourn time

        // Jitter sum from FlowMonitor is sum of absolute differences between consecutive packet arrival delays
        double jitterMs = (i->second.rxPackets > 1) ? (i->second.jitterSum.GetSeconds () / (i->second.rxPackets - 1)) * 1000 : 0.0;
        NS_LOG_INFO ("    Jitter: " << jitterMs << " ms");

        totalThroughputMbps += throughputMbps;
        totalRxPackets += i->second.rxPackets;
        totalDelaySum += i->second.delaySum;
        totalJitterSum += i->second.jitterSum;
        activeFlowCount++;
      }
      else
      {
          NS_LOG_INFO ("    No packets received for this flow.");
      }
    }
  }

  NS_LOG_INFO ("\n--- Aggregate Flow Statistics (for relevant flows) ---");
  if (activeFlowCount > 0)
  {
      NS_LOG_INFO ("Total Average Throughput: " << totalThroughputMbps / activeFlowCount << " Mbps");
      NS_LOG_INFO ("Overall Mean Delay: " << (totalDelaySum.GetSeconds () / totalRxPackets) * 1000 << " ms");
      NS_LOG_INFO ("Overall Mean Jitter: " << (totalJitterSum.GetSeconds () / (totalRxPackets - activeFlowCount)) * 1000 << " ms");
  }
  else
  {
      NS_LOG_INFO ("No TCP flow statistics found between Node 0 and Node 1.");
  }

  // Optionally, serialize FlowMonitor data to XML for external analysis
  // flowMonitor.SerializeToXmlFile ("red_queue_disc_flow_monitor.xml", true, true);

  // 10. Clean up
  NS_LOG_INFO ("Destroying simulator.");
  Simulator::Destroy ();

  return 0;
}

} // namespace ns3