#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/red-queue.h"
#include "ns3/nls-red-queue.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellRedNls");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  int nLeafNodes = 10;
  std::string queueDiscType = "RED"; // RED or NLRED
  uint32_t queueDiscLimit = 20; //packets
  uint32_t packetSize = 1024;
  std::string dataRate = "10Mbps";
  std::string delay = "20ms";
  uint32_t redMinTh = 5;
  uint32_t redMaxTh = 15;
  double redLinkBandwidth = 10; //Mbps
  double redQueueWeight = 0.002;
  bool byteMode = false;

  cmd.AddValue ("nLeafNodes", "Number of leaf nodes", nLeafNodes);
  cmd.AddValue ("queueDiscType", "Queue disc type (RED or NLRED)", queueDiscType);
  cmd.AddValue ("queueDiscLimit", "Queue disc limit (packets)", queueDiscLimit);
  cmd.AddValue ("packetSize", "Packet size", packetSize);
  cmd.AddValue ("dataRate", "Data rate", dataRate);
  cmd.AddValue ("delay", "Delay", delay);
  cmd.AddValue ("redMinTh", "RED minimum threshold", redMinTh);
  cmd.AddValue ("redMaxTh", "RED maximum threshold", redMaxTh);
  cmd.AddValue ("redLinkBandwidth", "RED link bandwidth (Mbps)", redLinkBandwidth);
  cmd.AddValue ("redQueueWeight", "RED queue weight", redQueueWeight);
  cmd.AddValue ("byteMode", "Use byte mode (true/false)", byteMode);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetLogLevel (queueDiscType == "RED" ? "RedQueue" : "NlsRedQueue", LogLevel::LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer leftRouters, rightRouters, leftLeafNodes, rightLeafNodes;
  leftRouters.Create (1);
  rightRouters.Create (1);
  leftLeafNodes.Create (nLeafNodes);
  rightLeafNodes.Create (nLeafNodes);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install (leftRouters);
  stack.Install (rightRouters);
  stack.Install (leftLeafNodes);
  stack.Install (rightLeafNodes);

  // Create point-to-point helper
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  // Connect leaf nodes to routers
  NetDeviceContainer leftLeafDevices, rightLeafDevices;
  for (int i = 0; i < nLeafNodes; ++i)
  {
    NetDeviceContainer devices;
    devices = pointToPoint.Install (leftLeafNodes.Get (i), leftRouters.Get (0));
    leftLeafDevices.Add (devices.Get (0));
    leftLeafDevices.Add (devices.Get (1));

    devices = pointToPoint.Install (rightLeafNodes.Get (i), rightRouters.Get (0));
    rightLeafDevices.Add (devices.Get (0));
    rightLeafDevices.Add (devices.Get (1));
  }

  // Connect routers
  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer routerDevices;
  routerDevices = bottleneckLink.Install (leftRouters.Get (0), rightRouters.Get (0));

  // Assign addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftLeafInterfaces = address.Assign (leftLeafDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer rightLeafInterfaces = address.Assign (rightLeafDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces = address.Assign (routerDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure queue disc
  TrafficControlHelper tch;
  tch.SetQueueDisc ("ns3::" + queueDiscType,
                     "MaxSize", StringValue (std::to_string (queueDiscLimit) + (byteMode ? "b" : "p")));

  if (queueDiscType == "RED" || queueDiscType == "NlsRedQueue") {
    tch.SetQueueDiscAttribute("LinkBandwidth", StringValue(dataRate));
    tch.SetQueueDiscAttribute ("MeanPktSize", UintegerValue (packetSize));

    if (byteMode) {
        tch.SetQueueDiscAttribute ("MeanPktSize", UintegerValue (packetSize));
    }

    if (queueDiscType == "RED") {
         tch.SetQueueDiscAttribute ("QueueWeight", DoubleValue (redQueueWeight));
         tch.SetQueueDiscAttribute ("Gentle", BooleanValue (false));
    }
     tch.SetQueueDiscAttribute ("MinTh", StringValue(std::to_string(redMinTh) + (byteMode ? "b" : "p")));
     tch.SetQueueDiscAttribute ("MaxTh", StringValue(std::to_string(redMaxTh) + (byteMode ? "b" : "p")));
  }

  QueueDiscContainer queueDiscs = tch.Install (routerDevices.Get (0));

  // Install OnOff application
  ApplicationContainer onOffApps;
  for (int i = 0; i < nLeafNodes; ++i)
  {
    OnOffHelper onOffHelper ("ns3::UdpSocketFactory", InetSocketAddress (rightLeafInterfaces.GetAddress (i), 9));
    onOffHelper.SetConstantRate (DataRate ("1Mbps"), packetSize);
    onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.1|Max=0.2]"));
    onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.8|Max=1.2]"));
    onOffApps.Add (onOffHelper.Install (leftLeafNodes.Get (i)));
  }

  onOffApps.Start (Seconds (1.0));
  onOffApps.Stop (Seconds (10.0));

  // Install sink application
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApps;
  for (int i = 0; i < nLeafNodes; ++i) {
      sinkApps.Add (sinkHelper.Install (rightLeafNodes.Get(i)));
  }
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  // Set simulation stop time
  Simulator::Stop (Seconds (10.0));

  // Enable Tracing
  // AsciiTraceHelper ascii;
  // pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("dumbbell.tr"));

  // Enable pcap tracing
  // pointToPoint.EnablePcapAll ("dumbbell");

  // Run simulation
  Simulator::ScheduleDestroy (&queueDiscs); // Make sure queueDiscs is destroyed correctly
  Simulator::Run ();

  // Print queue statistics
  Ptr<QueueDisc> queueDisc = queueDiscs.Get (0);
  if (queueDisc)
  {
    std::cout << "Queue Disc Statistics:" << std::endl;
    std::cout << "  Queue Disc Type: " << queueDiscType << std::endl;

    if (queueDiscType == "RED")
    {
      Ptr<RedQueueDisc> redQueue = DynamicCast<RedQueueDisc> (queueDisc);
      if (redQueue)
      {
        std::cout << "  DropPktEarly: " << redQueue->GetStats ().dropPktEarly << std::endl;
        std::cout << "  DropPktMark: " << redQueue->GetStats ().dropPktMark << std::endl;
        std::cout << "  DropPktOther: " << redQueue->GetStats ().dropPktOther << std::endl;
        std::cout << "  MarkPkt: " << redQueue->GetStats ().markPkt << std::endl;
      }
      else
      {
        std::cerr << "Error: Could not cast QueueDisc to RedQueueDisc." << std::endl;
      }
    }
      else if (queueDiscType == "NlsRedQueue") {
      Ptr<NlsRedQueueDisc> nlsRedQueue = DynamicCast<NlsRedQueueDisc> (queueDisc);
      if (nlsRedQueue)
      {
        std::cout << "  DropPktEarly: " << nlsRedQueue->GetStats ().dropPktEarly << std::endl;
        std::cout << "  DropPktMark: " << nlsRedQueue->GetStats ().dropPktMark << std::endl;
        std::cout << "  DropPktOther: " << nlsRedQueue->GetStats ().dropPktOther << std::endl;
        std::cout << "  MarkPkt: " << nlsRedQueue->GetStats ().markPkt << std::endl;
      }
      else
      {
        std::cerr << "Error: Could not cast QueueDisc to NlsRedQueueDisc." << std::endl;
      }
    } else {
        Ptr<DropTailQueue> dropTailQueue = DynamicCast<DropTailQueue>(queueDisc);
        if(dropTailQueue) {
            std::cout << "  Packets Dropped: " << dropTailQueue->GetStats().packetsDropped << std::endl;
        }
        else {
            std::cerr << "Error: Could not cast QueueDisc to DropTailQueue." << std::endl;
        }
    }

  }
  else
  {
    std::cerr << "Error: Could not retrieve QueueDisc." << std::endl;
  }

  Simulator::Destroy ();
  return 0;
}