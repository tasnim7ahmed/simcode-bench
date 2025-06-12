#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Dumbbell");

int main (int argc, char *argv[])
{
  uint32_t nLeafNodes = 3;
  uint32_t queueSizePackets = 100;
  uint32_t packetSize = 1024;
  std::string dataRate = "10Mbps";
  std::string queueType = "RED";
  std::string linkBandwidth = "100Mbps";
  std::string linkDelay = "2ms";
  double redMinTh = 15;
  double redMaxTh = 45;
  double redQueueWeight = 0.002;

  CommandLine cmd;
  cmd.AddValue ("nLeafNodes", "Number of leaf nodes", nLeafNodes);
  cmd.AddValue ("queueSize", "Queue size in packets", queueSizePackets);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("dataRate", "Data rate of OnOff application", dataRate);
  cmd.AddValue ("queueType", "Queue type (RED or ARED)", queueType);
  cmd.AddValue ("linkBandwidth", "Bandwidth of point-to-point links", linkBandwidth);
  cmd.AddValue ("linkDelay", "Delay of point-to-point links", linkDelay);
  cmd.AddValue ("redMinTh", "RED minimum threshold", redMinTh);
  cmd.AddValue ("redMaxTh", "RED maximum threshold", redMaxTh);
  cmd.AddValue ("redQueueWeight", "RED queue weight", redQueueWeight);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("0.1s"));

  NodeContainer leftLeafNodes, rightLeafNodes, routerNodes;
  leftLeafNodes.Create (nLeafNodes);
  rightLeafNodes.Create (nLeafNodes);
  routerNodes.Create (2);

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue (linkBandwidth));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue (linkDelay));

  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute ("DataRate", StringValue (linkBandwidth));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue (linkDelay));

  NetDeviceContainer leafRouterDevicesLeft[nLeafNodes];
  NetDeviceContainer leafRouterDevicesRight[nLeafNodes];
  NetDeviceContainer routerRouterDevices;

  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      leafRouterDevicesLeft[i] = pointToPointLeaf.Install (leftLeafNodes.Get (i), routerNodes.Get (0));
      leafRouterDevicesRight[i] = pointToPointLeaf.Install (rightLeafNodes.Get (i), routerNodes.Get (1));
    }
  routerRouterDevices = pointToPointRouter.Install (routerNodes.Get (0), routerNodes.Get (1));

  InternetStackHelper stack;
  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      stack.Install (leftLeafNodes.Get (i));
      stack.Install (rightLeafNodes.Get (i));
    }
  stack.Install (routerNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leafRouterIfLeft[nLeafNodes];
  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      leafRouterIfLeft[i] = address.Assign (leafRouterDevicesLeft[i]);
      address.NewNetwork ();
    }

  Ipv4AddressHelper address2;
  address2.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer leafRouterIfRight[nLeafNodes];
  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      leafRouterIfRight[i] = address2.Assign (leafRouterDevicesRight[i]);
      address2.NewNetwork ();
    }

  Ipv4AddressHelper address3;
  address3.SetBase ("10.1.100.0", "255.255.255.0");
  Ipv4InterfaceContainer routerRouterIf = address3.Assign (routerRouterDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  TrafficControlLayer::SetRootQueueDisc ("ns3::FifoQueueDisc",
                                         "MaxSize", StringValue (std::to_string(queueSizePackets) + "p"));

  TypeId queueTypeId;
  if (queueType == "RED")
    {
      queueTypeId = TypeId::LookupByName ("ns3::RedQueueDisc");
    }
  else if (queueType == "ARED")
    {
      queueTypeId = TypeId::LookupByName ("ns3::AdaptiveRedQueueDisc");
    }
  else
    {
      std::cerr << "Invalid queue type.  Must be RED or ARED." << std::endl;
      return 1;
    }

  QueueDiscContainer queueDiscs;
  for (uint32_t i = 0; i < 2; ++i)
    {
      ObjectFactory queueFactory;
      queueFactory.SetTypeId (queueTypeId);
      queueFactory.Set ("LinkBandwidth", StringValue (linkBandwidth));
      queueFactory.Set ("LinkDelay", StringValue (linkDelay));
      queueFactory.Set ("QueueLimit", StringValue (std::to_string(queueSizePackets) + "p"));
      if(queueType == "RED") {
        queueFactory.Set ("MeanPktSize", UintegerValue (packetSize));
        queueFactory.Set ("RedMinTh", DoubleValue (redMinTh));
        queueFactory.Set ("RedMaxTh", DoubleValue (redMaxTh));
        queueFactory.Set ("RedQueueWeight", DoubleValue (redQueueWeight));
      }
      Ptr<QueueDisc> queue = queueFactory.Create<QueueDisc> ();
      Names::Add ("/NodeList/" + std::to_string(routerNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::PointToPointNetDevice/TrafficControl", queue);
      queueDiscs.Add(queue);
    }

  uint16_t port = 9;

  for (uint32_t i = 0; i < nLeafNodes; ++i)
    {
      OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (leafRouterIfLeft[i].GetAddress (0), port)));
      onoff.SetConstantRate (DataRate (dataRate), packetSize);

      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
      var->SetAttribute ("Min", DoubleValue (0.1));
      var->SetAttribute ("Max", DoubleValue (1.0));
      onoff.SetAttribute ("OnTime", PointerValue (var));
      onoff.SetAttribute ("OffTime", PointerValue (var));
      ApplicationContainer app = onoff.Install (rightLeafNodes.Get (i));
      app.Start (Seconds (1.0));
      app.Stop (Seconds (10.0));
    }

  Simulator::Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    }

  for (uint32_t i = 0; i < 2; ++i)
  {
      Ptr<QueueDisc> queue = StaticCast<QueueDisc> (Names::Find<Object>("/NodeList/" + std::to_string(routerNodes.Get(i)->GetId()) + "/DeviceList/*/$ns3::PointToPointNetDevice/TrafficControl"));
      if (queue != NULL) {
          std::cout << "Queue Disc Statistics for Router " << i << ":\n";
          queue->PrintStatistics (std::cout);
      } else {
          std::cout << "Queue Disc not found for Router " << i << std::endl;
      }
  }

  Simulator::Destroy ();

  return 0;
}