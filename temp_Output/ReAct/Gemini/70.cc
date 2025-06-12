#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  int nLeafNodes = 10;
  std::string queueDiscType = "Red";
  std::string queueMode = "packets";
  uint32_t maxBytes = 3000000;
  uint32_t maxPackets = 1000;
  uint32_t meanPktSize = 1000;
  std::string dataRate = "10Mbps";
  std::string delay = "20ms";
  uint32_t redMinTh = 15;
  uint32_t redMaxTh = 45;
  double redMarkProb = 0.1;
  double simulationTime = 10.0;
  bool enablePcap = false;

  cmd.AddValue ("nLeafNodes", "Number of leaf nodes", nLeafNodes);
  cmd.AddValue ("queueDiscType", "Queue disc type (Red or Nlred)", queueDiscType);
  cmd.AddValue ("queueMode", "Queue mode (bytes or packets)", queueMode);
  cmd.AddValue ("maxBytes", "Max bytes in queue", maxBytes);
  cmd.AddValue ("maxPackets", "Max packets in queue", maxPackets);
  cmd.AddValue ("meanPktSize", "Mean packet size", meanPktSize);
  cmd.AddValue ("dataRate", "Data rate of bottleneck link", dataRate);
  cmd.AddValue ("delay", "Delay of bottleneck link", delay);
  cmd.AddValue ("redMinTh", "RED min threshold", redMinTh);
  cmd.AddValue ("redMaxTh", "RED max threshold", redMaxTh);
  cmd.AddValue ("redMarkProb", "RED marking probability", redMarkProb);
  cmd.AddValue ("simulationTime", "Simulation time", simulationTime);
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer leftRouters, rightRouters, leftLeafNodes, rightLeafNodes;
  leftRouters.Create (1);
  rightRouters.Create (1);
  leftLeafNodes.Create (nLeafNodes);
  rightLeafNodes.Create (nLeafNodes);

  InternetStackHelper stack;
  stack.Install (leftRouters);
  stack.Install (rightRouters);
  stack.Install (leftLeafNodes);
  stack.Install (rightLeafNodes);

  PointToPointHelper leafLink;
  leafLink.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  leafLink.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper backboneLink;
  backboneLink.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  backboneLink.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer leftLeafDevices, rightLeafDevices, backboneDevices;

  for (int i = 0; i < nLeafNodes; ++i)
  {
    NetDeviceContainer devices;
    devices = leafLink.Install (leftRouters.Get (0), leftLeafNodes.Get (i));
    leftLeafDevices.Add (devices.Get (1));

    devices = leafLink.Install (rightRouters.Get (0), rightLeafNodes.Get (i));
    rightLeafDevices.Add (devices.Get (1));
  }

  backboneDevices = backboneLink.Install (leftRouters.Get (0), rightRouters.Get (0));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftRouterInterfaces;
  for (int i = 0; i < nLeafNodes; ++i)
  {
    leftRouterInterfaces = address.Assign (leftLeafDevices.Get (i));
    address.NewNetwork ();
  }

  address.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer rightRouterInterfaces;
  for (int i = 0; i < nLeafNodes; ++i)
  {
    rightRouterInterfaces = address.Assign (rightLeafDevices.Get (i));
    address.NewNetwork ();
  }

  address.SetBase ("10.3.1.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneInterfaces = address.Assign (backboneDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure Queue Disc
  TypeId queueDiscTypeId;
  if (queueDiscType == "Red")
  {
    queueDiscTypeId = TypeId::LookupByName ("ns3::RedQueueDisc");
  }
  else if (queueDiscType == "Nlred")
  {
    queueDiscTypeId = TypeId::LookupByName ("ns3::NlredQueueDisc");
  }
  else
  {
    NS_FATAL_ERROR ("Invalid queue disc type: " << queueDiscType);
  }

  QueueDiscContainer queueDiscs;
  QueueDiscHelper queueDiscHelper;
  queueDiscHelper.SetType (queueDiscTypeId);
  if (queueMode == "bytes")
  {
    queueDiscHelper.SetAttribute ("Mode", StringValue ("MODE_BYTES"));
    queueDiscHelper.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  }
  else if (queueMode == "packets")
  {
    queueDiscHelper.SetAttribute ("Mode", StringValue ("MODE_PACKETS"));
    queueDiscHelper.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  }
  else
  {
    NS_FATAL_ERROR ("Invalid queue mode: " << queueMode);
  }

  queueDiscHelper.SetAttribute ("MeanPktSize", UintegerValue (meanPktSize));
  if (queueDiscType == "Red")
  {
    queueDiscHelper.SetAttribute ("LinkBandwidth", StringValue (dataRate));
    queueDiscHelper.SetAttribute ("LinkDelay", StringValue (delay));
    queueDiscHelper.SetAttribute ("Qlim", UintegerValue (1000));
    queueDiscHelper.SetAttribute ("MinTh", DoubleValue (redMinTh));
    queueDiscHelper.SetAttribute ("MaxTh", DoubleValue (redMaxTh));
    queueDiscHelper.SetAttribute ("MarkProb", DoubleValue (redMarkProb));
  }

  queueDiscs = queueDiscHelper.Install (backboneDevices.Get (0));

  // Configure OnOff Application
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress (backboneInterfaces.GetAddress (1), 9)));
  onOffHelper.SetConstantRate (DataRate ("1Mbps"));

  ApplicationContainer onOffApplications;
  for (int i = 0; i < nLeafNodes; ++i)
  {
    onOffApplications.Add (onOffHelper.Install (rightLeafNodes.Get (i)));
  }

  onOffApplications.Start (Seconds (1.0));
  onOffApplications.Stop (Seconds (simulationTime - 1.0));

  // Configure Sink Application
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApplications = sinkHelper.Install (leftLeafNodes);
  sinkApplications.Start (Seconds (1.0));
  sinkApplications.Stop (Seconds (simulationTime));

  // Configure Flow Monitor
  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll ();

  //Enable Tracing
  if (enablePcap)
  {
    PointToPointHelper p2p;
    p2p.EnablePcapAll ("dumbbell");
  }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
  }

  Ptr<QueueDisc> queueDisc = queueDiscs.Get (0);
  if (queueDisc)
  {
      std::cout << "Queue Disc Stats:\n";
      if (queueDiscType == "Red")
      {
          Ptr<RedQueueDisc> redQueueDisc = DynamicCast<RedQueueDisc>(queueDisc);
          if (redQueueDisc)
          {
              std::cout << "  Drop Count: " << redQueueDisc->GetDropCount() << "\n";
              std::cout << "  Mark Count: " << redQueueDisc->GetMarkCount() << "\n";
          }
          else
          {
              std::cerr << "Error: Could not cast QueueDisc to RedQueueDisc.\n";
          }

      }
      else if(queueDiscType == "Nlred")
      {
          Ptr<NlredQueueDisc> nlredQueueDisc = DynamicCast<NlredQueueDisc>(queueDisc);
          if (nlredQueueDisc)
          {
            std::cout << "  Drop Count: " << nlredQueueDisc->GetDropCount() << "\n";
            std::cout << "  Mark Count: " << nlredQueueDisc->GetMarkCount() << "\n";
          }
          else
          {
              std::cerr << "Error: Could not cast QueueDisc to NlredQueueDisc.\n";
          }
      }
  }
  else
  {
      std::cerr << "Error: QueueDisc not found.\n";
  }

  Simulator::Destroy ();

  return 0;
}