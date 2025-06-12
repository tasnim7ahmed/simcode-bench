#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellTopology");

int
main (int argc, char *argv[])
{
  // Default values
  uint32_t nLeafNodes = 3;
  std::string queueType = "RED";
  uint32_t maxPackets = 20;
  uint32_t packetSize = 1024;
  std::string dataRate = "10Mbps";
  std::string linkDelay = "2ms";
  std::string redMeanPktSize = "1024";  // Not used but kept for possible future adaptive RED extensions
  double redQW = 0.002;
  uint32_t redMinTh = 5;
  uint32_t redMaxTh = 15;

  // Enable command line arguments
  CommandLine cmd;
  cmd.AddValue ("nLeafNodes", "Number of leaf nodes", nLeafNodes);
  cmd.AddValue ("queueType", "Queue type (RED or AdaptiveRED)", queueType);
  cmd.AddValue ("maxPackets", "Max packets in queue", maxPackets);
  cmd.AddValue ("packetSize", "Packet size", packetSize);
  cmd.AddValue ("dataRate", "Data rate of links", dataRate);
  cmd.AddValue ("linkDelay", "Delay of links", linkDelay);
  cmd.AddValue ("redMeanPktSize", "Mean packet size for RED", redMeanPktSize); // Keeping this for future adaptation
  cmd.AddValue ("redQW", "RED queue weight", redQW);
  cmd.AddValue ("redMinTh", "RED min threshold", redMinTh);
  cmd.AddValue ("redMaxTh", "RED max threshold", redMaxTh);
  cmd.Parse (argc, argv);

  // Check if the queue type is valid.  If it's AdaptiveRED, ensure the dependencies for AdaptiveRED are present
  if (queueType != "RED" && queueType != "AdaptiveRED")
    {
      std::cerr << "Invalid queue type.  Must be RED or AdaptiveRED." << std::endl;
      return 1;
    }

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);


  // Create nodes
  NodeContainer leftRouters, rightRouters, leafNodesLeft, leafNodesRight;
  leftRouters.Create (1);
  rightRouters.Create (1);
  leafNodesLeft.Create (nLeafNodes);
  leafNodesRight.Create (nLeafNodes);

  // Create point-to-point helper
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (linkDelay));

  // Configure queue
  QueueDiscContainer queueDiscs;
  if (queueType == "RED")
    {
      TrafficControlHelper tch;
      tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue (dataRate),
                             "LinkDelay", StringValue (linkDelay),
                             "QueueLimit", UintegerValue (maxPackets),
                             "MeanPktSize", StringValue (redMeanPktSize),
                             "qW", DoubleValue (redQW),
                             "minTh", UintegerValue (redMinTh),
                             "maxTh", UintegerValue (redMaxTh));
      QueueDiscContainer qdiscs = tch.Install (rightRouters);
      queueDiscs.Add (qdiscs.Get (0));
    }
  else //AdaptiveRED
    {
      TrafficControlHelper tch;
      tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue (dataRate),
                             "LinkDelay", StringValue (linkDelay),
                             "QueueLimit", UintegerValue (maxPackets),
                             "MeanPktSize", StringValue (redMeanPktSize),
                             "qW", DoubleValue (redQW),
                             "minTh", UintegerValue (redMinTh),
                             "maxTh", UintegerValue (redMaxTh));

      QueueDiscContainer qdiscs = tch.Install (rightRouters);
      queueDiscs.Add (qdiscs.Get (0));
    }


  // Create links
  NetDeviceContainer leftRouterDevices = pointToPoint.Install (leftRouters, leafNodesLeft);
  NetDeviceContainer rightRouterDevices = pointToPoint.Install (rightRouters, leafNodesRight);
  NetDeviceContainer dumbbellLink = pointToPoint.Install (leftRouters, rightRouters);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install (leafNodesLeft);
  internet.Install (leafNodesRight);
  internet.Install (leftRouters);
  internet.Install (rightRouters);

  // Assign addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftInterfaces = address.Assign (leftRouterDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer rightInterfaces = address.Assign (rightRouterDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer dumbbellInterfaces = address.Assign (dumbbellLink);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install OnOff application on right side nodes
  uint16_t port = 9; // Discard port
  OnOffHelper onoff ("ns3::UdpClient", Address (InetSocketAddress (rightInterfaces.GetAddress (0), port)));
  onoff.SetConstantRate (DataRate ("1Mbps"));
  ApplicationContainer apps;

  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
      var->SetAttribute ("Min", DoubleValue (0.1));
      var->SetAttribute ("Max", DoubleValue (1.0));
      onoff.SetAttribute ("OnTime", PointerValue (var));
      onoff.SetAttribute ("OffTime", PointerValue (var));
      apps.Add (onoff.Install (leafNodesRight.Get (i)));
    }
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Install sink application on left side nodes
  UdpServerHelper echoServer (port);
  ApplicationContainer sinkApps = echoServer.Install (leafNodesLeft);
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (10.0));

  // Animation
  //  AnimationInterface anim("dumbbell.xml");
  //  anim.EnablePacketMetadata ();

  Simulator::Run ();

  // Print statistics
  monitor->CheckForLostPackets ();
  Ptr<IQueueDisc> queue = DynamicCast<IQueueDisc> (queueDiscs.Get (0));
  if (queue)
    {
      std::cout << "Queue Disc statistics:" << std::endl;
      queue->GetStats ()->Print ();

       RedQueueDisc::Stats st = queue->GetStats<RedQueueDisc::Stats> ();

       std::cout << "  AvgQueueLen: " << st.avgQueueLen << std::endl;
       std::cout << "  QueueLimit: " << st.queueLimit << std::endl;
       std::cout << "  MeanPktSize: " << st.meanPktSize << std::endl;
       std::cout << "  qW: " << st.qW << std::endl;
       std::cout << "  minTh: " << st.minTh << std::endl;
       std::cout << "  maxTh: " << st.maxTh << std::endl;
       std::cout << "  pktsDropped.DropBytes: " << st.pktsDropped.DropBytes << std::endl;
       std::cout << "  pktsDropped.DropPackets: " << st.pktsDropped.DropPackets << std::endl;

      if (st.pktsDropped.DropPackets > 0)
      {
        std::cout << "Packets were dropped!" << std::endl;
      }
      else
      {
        std::cout << "No packets were dropped!" << std::endl;
      }

    }
  else
    {
      std::cout << "Queue Disc is not IQueueDisc." << std::endl;
    }


  Simulator::Destroy ();

  return 0;
}