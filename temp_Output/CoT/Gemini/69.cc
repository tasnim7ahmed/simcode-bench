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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  int nLeafNodes = 3;
  std::string queueType = "RED";
  uint32_t maxPackets = 20;
  uint32_t packetSize = 512;
  std::string dataRate = "10Mbps";
  std::string queueLimit = "1000p";
  std::string delay = "2ms";
  double meanPktSize = 500;
  double qW = 0.002;
  double qTh_min = 5;
  double qTh_max = 15;
  double redMinTh = 5;
  double redMaxTh = 15;
  double redMaxP = 0.1;
  cmd.AddValue ("nLeafNodes", "Number of leaf nodes", nLeafNodes);
  cmd.AddValue ("queueType", "Queue Type (RED or Feng)", queueType);
  cmd.AddValue ("maxPackets", "Max Packets in Queue", maxPackets);
  cmd.AddValue ("packetSize", "Packet Size", packetSize);
  cmd.AddValue ("dataRate", "Data Rate", dataRate);
  cmd.AddValue ("queueLimit", "Queue Limit", queueLimit);
  cmd.AddValue ("delay", "Delay", delay);
  cmd.AddValue ("meanPktSize", "Mean Packet Size for Feng's Adaptive RED", meanPktSize);
  cmd.AddValue ("qW", "Queue Weight for Feng's Adaptive RED", qW);
  cmd.AddValue ("qTh_min", "Min Threshold for Feng's Adaptive RED", qTh_min);
  cmd.AddValue ("qTh_max", "Max Threshold for Feng's Adaptive RED", qTh_max);
  cmd.AddValue ("redMinTh", "Min Threshold for RED", redMinTh);
  cmd.AddValue ("redMaxTh", "Max Threshold for RED", redMaxTh);
  cmd.AddValue ("redMaxP", "Max P for RED", redMaxP);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetPrintfAll (true);

  NodeContainer leftLeafNodes;
  leftLeafNodes.Create (nLeafNodes);
  NodeContainer rightLeafNodes;
  rightLeafNodes.Create (nLeafNodes);
  NodeContainer routerNodes;
  routerNodes.Create (2);

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue (delay));

  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer leftLeafDevices;
  NetDeviceContainer rightLeafDevices;
  NetDeviceContainer routerDevices;

  for (int i = 0; i < nLeafNodes; ++i)
    {
      NetDeviceContainer link = pointToPointLeaf.Install (leftLeafNodes.Get (i), routerNodes.Get (0));
      leftLeafDevices.Add (link.Get (0));
      routerDevices.Add (link.Get (1));
    }

  for (int i = 0; i < nLeafNodes; ++i)
    {
      NetDeviceContainer link = pointToPointLeaf.Install (rightLeafNodes.Get (i), routerNodes.Get (1));
      rightLeafDevices.Add (link.Get (0));
      routerDevices.Add (link.Get (1));
    }

  NetDeviceContainer routerLink = pointToPointRouter.Install (routerNodes.Get (0), routerNodes.Get (1));

  InternetStackHelper stack;
  stack.Install (leftLeafNodes);
  stack.Install (rightLeafNodes);
  stack.Install (routerNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftLeafInterfaces = address.Assign (leftLeafDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer rightLeafInterfaces = address.Assign (rightLeafDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer routerInterfaces = address.Assign (routerDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer routerRouterInterfaces = address.Assign (routerLink);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  TrafficControlHelper tch;
  QueueDiscContainer qdiscs;

  if (queueType == "RED")
    {
      TypeId redQueueDiscTypeId = TypeId::LookupByName ("ns3::RedQueueDisc");
      tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue ("10Mbps"),
                             "LinkDelay", StringValue ("2ms"),
                             "QueueLimit", StringValue (queueLimit),
                             "RedMinTh", DoubleValue (redMinTh),
                             "RedMaxTh", DoubleValue (redMaxTh),
                             "RedMaxP", DoubleValue (redMaxP));
    }
  else if (queueType == "Feng")
    {
      TypeId fengQueueDiscTypeId = TypeId::LookupByName ("ns3::FengAdaptiveRedQueueDisc");
      tch.SetRootQueueDisc ("ns3::FengAdaptiveRedQueueDisc",
                             "LinkBandwidth", StringValue ("10Mbps"),
                             "LinkDelay", StringValue ("2ms"),
                             "QueueLimit", StringValue (queueLimit),
                             "MeanPktSize", DoubleValue (meanPktSize),
                             "qW", DoubleValue (qW),
                             "qTh_min", DoubleValue (qTh_min),
                             "qTh_max", DoubleValue (qTh_max));
    }
  else
    {
      std::cerr << "Invalid queue type.  Must be RED or Feng." << std::endl;
      return 1;
    }

  qdiscs = tch.Install (routerLink.Get (0));

  OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (rightLeafInterfaces.Get (0).GetAddress (), 9)));
  onoff.SetConstantRate (DataRate (dataRate), packetSize);

  for (int i = 0; i < nLeafNodes; ++i)
    {
      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
      var->SetAttribute ("Min", DoubleValue (0.1));
      var->SetAttribute ("Max", DoubleValue (1.0));
      ApplicationContainer app = onoff.Install (leftLeafNodes.Get (i));
      app.Start (Seconds (1.0 + var->GetValue ()));
      app.Stop (Seconds (10.0));

      Address sinkLocalAddress (InetSocketAddress (rightLeafInterfaces.Get (i).GetAddress (), 9));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkApp = sinkHelper.Install (rightLeafNodes.Get (i));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (10.0));
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  uint64_t packetsDropped = DynamicCast<RedQueueDisc> (qdiscs.Get (0))->GetStats ().drops;
  std::cout << "Packets Dropped: " << packetsDropped << std::endl;

  if (packetsDropped == 0)
    {
      std::cout << "No packets were dropped as expected." << std::endl;
    }
  else
    {
      std::cout << "Packets were dropped." << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}