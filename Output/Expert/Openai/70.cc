#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellRedNlredQueueDisc");

void
PrintQueueDiscStats (Ptr<QueueDisc> queue, std::string queueType)
{
  QueueDisc::Stats st = queue->GetStats ();

  std::cout << "QueueDisc (" << queueType << ") statistics:" << std::endl;
  std::cout << "  Packets enqueued: " << st.nTotalReceivedPackets << std::endl;
  std::cout << "  Bytes enqueued:   " << st.nTotalReceivedBytes << std::endl;
  std::cout << "  Packets dequeued: " << st.nTotalDequeuedPackets << std::endl;
  std::cout << "  Bytes dequeued:   " << st.nTotalDequeuedBytes << std::endl;
  std::cout << "  Packets dropped:  " << st.nTotalDroppedPackets << std::endl;
  std::cout << "  Bytes dropped:    " << st.nTotalDroppedBytes << std::endl;
  std::cout << "  Drops before enqueue:  " << st.nPacketsDroppedBeforeEnqueue << std::endl;
  std::cout << "  Drops after dequeue:   " << st.nPacketsDroppedAfterDequeue << std::endl;
}

int
main (int argc, char *argv[])
{
  uint32_t nLeaf = 4;
  std::string queueType = "RED";
  std::string queueMode = "Packets";
  uint32_t queueDiscLimitPackets = 100;
  uint32_t queueDiscLimitBytes = 64000;
  uint32_t pktSize = 1000; // bytes
  std::string bottleneckBw = "10Mbps";
  std::string bottleneckDelay = "5ms";
  std::string leafBw = "100Mbps";
  std::string leafDelay = "1ms";
  uint32_t maxTh = 50;
  uint32_t minTh = 20;
  double redQW = 0.002;
  double interval = 0.01;
  double maxp = 1.0;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("nLeaf", "Number of left/right leaf nodes", nLeaf);
  cmd.AddValue ("queueType", "QueueDisc type: RED or NLRED", queueType);
  cmd.AddValue ("queueMode", "QueueDisc mode: Packets or Bytes", queueMode);
  cmd.AddValue ("queueDiscLimitPackets", "Limit (in packets) for queueDisc if in packet mode", queueDiscLimitPackets);
  cmd.AddValue ("queueDiscLimitBytes", "Limit (in bytes) for queueDisc if in byte mode", queueDiscLimitBytes);
  cmd.AddValue ("pktSize", "Packet size (bytes)", pktSize);
  cmd.AddValue ("bottleneckBw", "Bottleneck link data rate", bottleneckBw);
  cmd.AddValue ("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
  cmd.AddValue ("leafBw", "Leaf link data rate", leafBw);
  cmd.AddValue ("leafDelay", "Leaf link delay", leafDelay);
  cmd.AddValue ("minTh", "RED MinTh", minTh);
  cmd.AddValue ("maxTh", "RED MaxTh", maxTh);
  cmd.AddValue ("redQW", "RED EWMA weight", redQW);
  cmd.AddValue ("interval", "RED interval", interval);
  cmd.AddValue ("maxp", "RED maxp", maxp);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.Parse (argc, argv);

  if (queueType != "RED" && queueType != "NLRED")
    {
      std::cerr << "Invalid queueType. Allowed: RED or NLRED" << std::endl;
      return 1;
    }

  if (queueMode != "Packets" && queueMode != "Bytes")
    {
      std::cerr << "Invalid queueMode. Allowed: Packets or Bytes" << std::endl;
      return 1;
    }

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));

  PointToPointHelper leaf;
  leaf.SetDeviceAttribute ("DataRate", StringValue (leafBw));
  leaf.SetChannelAttribute ("Delay", StringValue (leafDelay));

  PointToPointHelper bottleneck;
  bottleneck.SetDeviceAttribute ("DataRate", StringValue (bottleneckBw));
  bottleneck.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

  InternetStackHelper stack;

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc (queueType == "RED" ? "ns3::RedQueueDisc" : "ns3::NlRedQueueDisc");

  if (queueMode == "Packets")
    {
      tch.SetQueueLimits ("ns3::QueueLimits",
        "MaxPackets", UintegerValue (queueDiscLimitPackets));
    }
  else
    {
      tch.SetQueueLimits ("ns3::QueueLimits",
        "MaxBytes", UintegerValue (queueDiscLimitBytes));
    }

  if (queueType == "RED" || queueType == "NLRED")
    {
      std::string discType = queueType == "RED" ? "ns3::RedQueueDisc" : "ns3::NlRedQueueDisc";
      tch.SetRootQueueDisc (discType);

      if (queueMode == "Packets")
        {
          tch.SetQueueDiscAttribute ("Mode", StringValue ("QUEUE_DISC_MODE_PACKETS"));
          tch.SetQueueDiscAttribute ("MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
          tch.SetQueueDiscAttribute ("MinTh", DoubleValue (minTh));
          tch.SetQueueDiscAttribute ("MaxTh", DoubleValue (maxTh));
        }
      else
        {
          tch.SetQueueDiscAttribute ("Mode", StringValue ("QUEUE_DISC_MODE_BYTES"));
          tch.SetQueueDiscAttribute ("MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, queueDiscLimitBytes)));
          tch.SetQueueDiscAttribute ("MinTh", DoubleValue (minTh * pktSize));
          tch.SetQueueDiscAttribute ("MaxTh", DoubleValue (maxTh * pktSize));
        }
      tch.SetQueueDiscAttribute ("QW", DoubleValue (redQW));
      tch.SetQueueDiscAttribute ("Interval", TimeValue (Seconds (interval)));
      tch.SetQueueDiscAttribute ("MaxP", DoubleValue (maxp));
    }

  NodeContainer leftNodes, rightNodes, routers;
  leftNodes.Create (nLeaf);
  rightNodes.Create (nLeaf);
  routers.Create (2);

  NodeContainer leftRouter (leftNodes.GetN () + 1);
  for (uint32_t i = 0; i < leftNodes.GetN (); ++i)
    leftRouter.Add (leftNodes.Get (i));
  leftRouter.Add (routers.Get (0));

  NodeContainer rightRouter (rightNodes.GetN () + 1);
  for (uint32_t i = 0; i < rightNodes.GetN (); ++i)
    rightRouter.Add (rightNodes.Get (i));
  rightRouter.Add (routers.Get (1));

  // Install links between left leaf and left router
  NetDeviceContainer leftDevices, leftRouterDevices;
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      NodeContainer pair (leftNodes.Get (i), routers.Get (0));
      NetDeviceContainer link = leaf.Install (pair);
      leftDevices.Add (link.Get (0));
      leftRouterDevices.Add (link.Get (1));
    }

  // Install links between right leaf and right router
  NetDeviceContainer rightDevices, rightRouterDevices;
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      NodeContainer pair (rightNodes.Get (i), routers.Get (1));
      NetDeviceContainer link = leaf.Install (pair);
      rightDevices.Add (link.Get (0));
      rightRouterDevices.Add (link.Get (1));
    }

  // Install bottleneck link
  NetDeviceContainer bottleneckDevices;
  NetDeviceContainer bn = bottleneck.Install (NodeContainer (routers.Get (0), routers.Get (1)));
  bottleneckDevices.Add (bn);

  // Install internet stack
  stack.Install (leftNodes);
  stack.Install (rightNodes);
  stack.Install (routers);

  // Assign IP addresses
  Ipv4AddressHelper address;
  char subnet[64];
  std::vector<Ipv4InterfaceContainer> leftIfs (nLeaf), rightIfs (nLeaf);

  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      sprintf (subnet, "10.1.%u.0", i + 1);
      address.SetBase (subnet, "255.255.255.0");
      leftIfs[i] = address.Assign (NetDeviceContainer (leftDevices.Get (i), leftRouterDevices.Get (i)));
      address.NewNetwork ();
    }

  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      sprintf (subnet, "10.2.%u.0", i + 1);
      address.SetBase (subnet, "255.255.255.0");
      rightIfs[i] = address.Assign (NetDeviceContainer (rightDevices.Get (i), rightRouterDevices.Get (i)));
      address.NewNetwork ();
    }

  address.SetBase ("10.3.1.0", "255.255.255.0");
  Ipv4InterfaceContainer routerIfs = address.Assign (bottleneckDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install queue disc on the bottleneck device (at routers.Get(0) tx->rx routers.Get(1))
  Ptr<NetDevice> router0BottleneckDev = bottleneckDevices.Get (0);
  Ptr<NetDevice> router1BottleneckDev = bottleneckDevices.Get (1);

  QueueDiscContainer qdiscs = tch.Install (router0BottleneckDev);

  NS_ASSERT_MSG (qdiscs.GetN () == 1, "Should have exactly one queue disc installed on bottleneck");

  Ptr<QueueDisc> qd = qdiscs.Get (0);

  // Install apps: OnOff on each right leaf node, sink on left leaf node
  ApplicationContainer sinks, sources;
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      // Install sink on left node
      Address sinkAddress (InetSocketAddress (leftIfs[i].GetAddress (0), 9000 + i));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
      sinks.Add (sinkHelper.Install (leftNodes.Get (i)));

      // Traffic from right to left
      OnOffHelper onoff ("ns3::TcpSocketFactory", sinkAddress);
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("20Mbps")));
      onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
      Ptr<ExponentialRandomVariable> onTime = CreateObject<ExponentialRandomVariable> ();
      onTime->SetAttribute ("Mean", DoubleValue (0.5));
      Ptr<ExponentialRandomVariable> offTime = CreateObject<ExponentialRandomVariable> ();
      offTime->SetAttribute ("Mean", DoubleValue (0.5));
      onoff.SetAttribute ("OnTime", PointerValue (onTime));
      onoff.SetAttribute ("OffTime", PointerValue (offTime));
      sources.Add (onoff.Install (rightNodes.Get (i)));
    }

  sinks.Start (Seconds (0.0));
  sinks.Stop (Seconds (simulationTime));
  sources.Start (Seconds (1.0));
  sources.Stop (Seconds (simulationTime));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  PrintQueueDiscStats (qd, queueType);

  Simulator::Destroy ();
  return 0;
}