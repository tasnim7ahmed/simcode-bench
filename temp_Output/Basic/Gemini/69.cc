#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue.h"
#include "ns3/red-queue.h"
#include "ns3/feng-adaptive-red-queue.h"
#include "ns3/random-variable-stream.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  int nLeftLeaf = 10;
  int nRightLeaf = 10;
  std::string queueType = "RED";
  uint32_t maxPackets = 200;
  uint32_t packetSize = 1024;
  std::string dataRate = "10Mbps";
  std::string linkDelay = "20ms";
  double redMinTh = 20;
  double redMaxTh = 60;
  double redQueueWeight = 0.002;
  double redLintermDivisor = 9;
  bool enablePcap = false;

  cmd.AddValue ("nLeftLeaf", "Number of left side leaf nodes", nLeftLeaf);
  cmd.AddValue ("nRightLeaf", "Number of right side leaf nodes", nRightLeaf);
  cmd.AddValue ("queueType", "Queue type (RED or AdaptiveRED)", queueType);
  cmd.AddValue ("maxPackets", "Max packets in queue", maxPackets);
  cmd.AddValue ("packetSize", "Packet size", packetSize);
  cmd.AddValue ("dataRate", "Data rate", dataRate);
  cmd.AddValue ("linkDelay", "Link delay", linkDelay);
  cmd.AddValue ("redMinTh", "RED min threshold", redMinTh);
  cmd.AddValue ("redMaxTh", "RED max threshold", redMaxTh);
  cmd.AddValue ("redQueueWeight", "RED queue weight", redQueueWeight);
  cmd.AddValue ("redLintermDivisor", "RED linterm divisor", redLintermDivisor);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);

  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer leftLeafNodes, rightLeafNodes, spokeNodes;
  leftLeafNodes.Create (nLeftLeaf);
  rightLeafNodes.Create (nRightLeaf);
  spokeNodes.Create (2);

  NodeContainer allNodes;
  allNodes.Add (leftLeafNodes);
  allNodes.Add (rightLeafNodes);
  allNodes.Add (spokeNodes);

  InternetStackHelper stack;
  stack.Install (allNodes);

  PointToPointHelper leafSpokeLink;
  leafSpokeLink.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  leafSpokeLink.SetChannelAttribute ("Delay", StringValue (linkDelay));

  TrafficControlHelper tch;
  QueueDiscContainer spokeQueues;

  if (queueType == "RED")
    {
      tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue (dataRate),
                             "LinkDelay", StringValue (linkDelay),
                             "QueueWeight", DoubleValue (redQueueWeight),
                             "MeanPktSize", UintegerValue (packetSize),
                             "Gentle", BooleanValue (true),
                             "Qlim", UintegerValue (maxPackets),
                             "MinTh", DoubleValue (redMinTh),
                             "MaxTh", DoubleValue (redMaxTh),
                             "LIntermDiv", DoubleValue (redLintermDivisor));
    }
  else if (queueType == "AdaptiveRED")
    {
      tch.SetRootQueueDisc ("ns3::FengAdaptiveRedQueueDisc",
                             "LinkBandwidth", StringValue (dataRate),
                             "LinkDelay", StringValue (linkDelay),
                             "QueueWeight", DoubleValue (redQueueWeight),
                             "MeanPktSize", UintegerValue (packetSize),
                             "Gentle", BooleanValue (true),
                             "Qlim", UintegerValue (maxPackets),
                             "MinTh", DoubleValue (redMinTh),
                             "MaxTh", DoubleValue (redMaxTh),
                             "LIntermDiv", DoubleValue (redLintermDivisor));
    }
  else
    {
      std::cerr << "Invalid queue type: " << queueType << std::endl;
      return 1;
    }

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftLeafInterfaces;
  for (int i = 0; i < nLeftLeaf; ++i)
    {
      NetDeviceContainer devices = leafSpokeLink.Install (leftLeafNodes.Get (i), spokeNodes.Get (0));
      leftLeafInterfaces.Add (address.Assign (devices));
      spokeQueues.Add (tch.Install (devices.Get (1)));
      address.NewNetwork ();
    }

  address.SetBase ("10.1.100.0", "255.255.255.0");
  Ipv4InterfaceContainer rightLeafInterfaces;
  for (int i = 0; i < nRightLeaf; ++i)
    {
      NetDeviceContainer devices = leafSpokeLink.Install (rightLeafNodes.Get (i), spokeNodes.Get (1));
      rightLeafInterfaces.Add (address.Assign (devices));
      spokeQueues.Add (tch.Install (devices.Get (1)));
      address.NewNetwork ();
    }

  PointToPointHelper spokeSpokeLink;
  spokeSpokeLink.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  spokeSpokeLink.SetChannelAttribute ("Delay", StringValue (linkDelay));
  NetDeviceContainer spokeSpokeDevices = spokeSpokeLink.Install (spokeNodes);
  spokeQueues.Add (tch.Install (spokeSpokeDevices.Get (0)));
  spokeQueues.Add (tch.Install (spokeSpokeDevices.Get (1)));

  address.SetBase ("10.1.200.0", "255.255.255.0");
  Ipv4InterfaceContainer spokeSpokeInterfaces = address.Assign (spokeSpokeDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install OnOff application on all right-side nodes
  UniformRandomVariable randVar;
  randVar.SetAttribute ("Min", DoubleValue (0.1));
  randVar.SetAttribute ("Max", DoubleValue (1.0));

  for (int i = 0; i < nRightLeaf; ++i)
    {
      OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (rightLeafInterfaces.GetAddress (i), 9)));
      onoff.SetAttribute ("OnTime",  StringValue ("ns3::UniformRandomVariable[Min=0.1|Max=1.0]"));
      onoff.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.1|Max=1.0]"));
      onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
      onoff.SetAttribute ("DataRate", StringValue (dataRate));
      ApplicationContainer apps = onoff.Install (leftLeafNodes.Get (i % nLeftLeaf)); // Send from left nodes to right nodes
      apps.Start (Seconds (1.0));
      apps.Stop (Seconds (10.0));

      UdpServerHelper echoServer (9);
      ApplicationContainer serverApps = echoServer.Install (rightLeafNodes.Get (i));
      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds (10.0));

    }

  if (enablePcap)
    {
      leafSpokeLink.EnablePcapAll ("dumbbell");
      spokeSpokeLink.EnablePcapAll ("dumbbell");
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  uint64_t totalEnqueued = 0;
  uint64_t totalDequeued = 0;
  uint64_t totalDropped = 0;

  for (uint32_t i = 0; i < spokeQueues.GetN (); ++i)
    {
      Ptr<QueueDisc> queue = spokeQueues.Get (i);
      totalEnqueued += queue->GetTotalPacketsEnqueued ();
      totalDequeued += queue->GetTotalPacketsDequeued ();
      totalDropped += queue->GetTotalPacketsDropped ();

      std::cout << "Queue " << i << ": Enqueued=" << queue->GetTotalPacketsEnqueued ()
                << ", Dequeued=" << queue->GetTotalPacketsDequeued ()
                << ", Dropped=" << queue->GetTotalPacketsDropped () << std::endl;
    }

  std::cout << "Total: Enqueued=" << totalEnqueued
            << ", Dequeued=" << totalDequeued
            << ", Dropped=" << totalDropped << std::endl;

  // Check for expected drops
  if (totalDropped == 0)
    {
      std::cout << "No packets were dropped.  Consider increasing traffic load "
                << "or decreasing queue size." << std::endl;
    }

  Simulator::Destroy ();

  return 0;
}