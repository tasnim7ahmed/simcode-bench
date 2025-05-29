#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("TcpL4Protocol", StringValue ("ns3::TcpNewReno"));

  NodeContainer senders, receivers, switches, routers;
  senders.Create (40);
  receivers.Create (40);
  switches.Create (2);
  routers.Create (1);

  NodeContainer allNodes;
  allNodes.Add (senders);
  allNodes.Add (receivers);
  allNodes.Add (switches);
  allNodes.Add (routers);

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");

  Ipv4InterfaceContainer senderInterfaces, receiverInterfaces, switchInterfaces, routerInterfaces;

  for (uint32_t i = 0; i < 30; ++i)
  {
    PointToPointHelper senderSwitchLink;
    senderSwitchLink.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
    senderSwitchLink.SetChannelAttribute ("Delay", StringValue ("100us"));
    NetDeviceContainer devices = senderSwitchLink.Install (senders.Get (i), switches.Get (0));
    senderInterfaces.Add (address.AssignOne (devices.Get (1)));
    address.NewNetwork ();
  }

  for (uint32_t i = 30; i < 40; ++i)
  {
    PointToPointHelper senderSwitchLink;
    senderSwitchLink.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
    senderSwitchLink.SetChannelAttribute ("Delay", StringValue ("100us"));
    NetDeviceContainer devices = senderSwitchLink.Install (senders.Get (i), switches.Get (1));
    senderInterfaces.Add (address.AssignOne (devices.Get (1)));
    address.NewNetwork ();
  }

  for (uint32_t i = 0; i < 40; ++i)
  {
    PointToPointHelper receiverSwitchLink;
    receiverSwitchLink.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
    receiverSwitchLink.SetChannelAttribute ("Delay", StringValue ("100us"));
    NetDeviceContainer devices = receiverSwitchLink.Install (receivers.Get (i), routers.Get (0));
    receiverInterfaces.Add (address.AssignOne (devices.Get (1)));
    address.NewNetwork ();
  }


  PointToPointHelper switchRouterLink1;
  switchRouterLink1.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  switchRouterLink1.SetChannelAttribute ("Delay", StringValue ("100us"));

  NetDeviceContainer switchRouterDevices1 = switchRouterLink1.Install (switches.Get (0), switches.Get (1));
  switchInterfaces.Add (address.AssignOne (switchRouterDevices1.Get (0)));
  switchInterfaces.Add (address.AssignOne (switchRouterDevices1.Get (1)));
  address.NewNetwork ();

  PointToPointHelper switchRouterLink2;
  switchRouterLink2.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  switchRouterLink2.SetChannelAttribute ("Delay", StringValue ("100us"));
  NetDeviceContainer switchRouterDevices2 = switchRouterLink2.Install (switches.Get (1), routers.Get (0));
  switchInterfaces.Add (address.AssignOne (switchRouterDevices2.Get (0)));
  routerInterfaces.Add (address.AssignOne (switchRouterDevices2.Get (1)));
  address.NewNetwork ();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  ApplicationContainer sinkApps;

  for (uint32_t i = 0; i < 40; ++i)
  {
    PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces.GetAddress (i, 0), port));
    sinkApps.Add (sink.Install (receivers.Get (i)));
  }

  sinkApps.Start (Seconds (3.0));
  sinkApps.Stop (Seconds (6.0));

  ApplicationContainer clientApps;

  for (uint32_t i = 0; i < 40; ++i)
  {
    BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (receiverInterfaces.GetAddress (i, 0), port));
    source.SetAttribute ("SendSize", UintegerValue (1448));
    clientApps.Add (source.Install (senders.Get (i)));
    clientApps.Get (i)->SetStartTime (Seconds (3.0 + (double)i/40.0));
    clientApps.Get (i)->SetStopTime (Seconds (6.0));
  }

  TrafficControlHelper tchBottleneck1, tchBottleneck2;
  tchBottleneck1.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue("10Gbps"),
                                   "LinkDelay", StringValue("100us"),
                                   "MeanPktSize", UintegerValue (1500));
  tchBottleneck2.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue("1Gbps"),
                                   "LinkDelay", StringValue("100us"),
                                   "MeanPktSize", UintegerValue (1500));

  QueueDiscContainer queueDiscsBottleneck1 = tchBottleneck1.Install (switchRouterDevices1.Get (0));
  QueueDiscContainer queueDiscsBottleneck2 = tchBottleneck2.Install (switchRouterDevices2.Get (0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (6.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalThroughput = 0.0;
  std::vector<double> flowThroughputs;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1e6;
    flowThroughputs.push_back(throughput);
    totalThroughput += throughput;
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") : Throughput: " << throughput << " Mbps" << std::endl;
  }

  double fairnessIndex = 0.0;
  double sumOfSquares = 0.0;
  double sum = 0.0;

  for (double throughput : flowThroughputs)
  {
    sum += throughput;
    sumOfSquares += throughput * throughput;
  }

  if (flowThroughputs.size() > 0)
  {
      fairnessIndex = (sum * sum) / (flowThroughputs.size() * sumOfSquares);
  }

  std::cout << "Aggregate Throughput: " << totalThroughput << " Mbps" << std::endl;
  std::cout << "Jain's Fairness Index: " << fairnessIndex << std::endl;

  Simulator::Destroy ();

  return 0;
}