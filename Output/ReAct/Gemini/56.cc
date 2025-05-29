#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_WARN);
  LogComponent::SetLogLevel ( "DctcpSimulation", LogLevel::LOG_LEVEL_INFO );

  // Create Nodes
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

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  // Create Links
  PointToPointHelper senderSwitchLink, switchSwitchLink, switchRouterLink, routerReceiverLink;

  // Sender-Switch Links (10 Gbps)
  senderSwitchLink.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  senderSwitchLink.SetChannelAttribute ("Delay", StringValue ("10us"));

  // Switch-Switch Link (10 Gbps)
  switchSwitchLink.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  switchSwitchLink.SetChannelAttribute ("Delay", StringValue ("10us"));

  // Switch-Router Links (1 Gbps)
  switchRouterLink.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  switchRouterLink.SetChannelAttribute ("Delay", StringValue ("10us"));

  // Router-Receiver Links (1 Gbps)
  routerReceiverLink.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  routerReceiverLink.SetChannelAttribute ("Delay", StringValue ("10us"));

  // Create NetDevices and Channels
  NetDeviceContainer senderSwitchDevices, switchSwitchDevices, switchRouterDevices, routerReceiverDevices;
  Ipv4InterfaceContainer senderSwitchInterfaces, switchSwitchInterfaces, switchRouterInterfaces, routerReceiverInterfaces;

  // Connect Senders to Switches
  for (uint32_t i = 0; i < 30; ++i)
    {
      NetDeviceContainer devices = senderSwitchLink.Install (senders.Get (i), switches.Get (0));
      senderSwitchDevices.Add (devices.Get (0));
      senderSwitchDevices.Add (devices.Get (1));
    }
  for (uint32_t i = 30; i < 40; ++i)
    {
      NetDeviceContainer devices = senderSwitchLink.Install (senders.Get (i), switches.Get (1));
      senderSwitchDevices.Add (devices.Get (0));
      senderSwitchDevices.Add (devices.Get (1));
    }

  // Connect Switches
  switchSwitchDevices = switchSwitchLink.Install (switches.Get (0), switches.Get (1));

  // Connect Switches to Router
  for (uint32_t i = 0; i < 20; ++i)
    {
      NetDeviceContainer devices = switchRouterLink.Install (switches.Get (1), routers.Get (0));
      switchRouterDevices.Add (devices.Get (0));
      switchRouterDevices.Add (devices.Get (1));
    }

  // Connect Router to Receivers
  for (uint32_t i = 0; i < 40; ++i)
    {
      NetDeviceContainer devices = routerReceiverLink.Install (routers.Get (0), receivers.Get (i));
      routerReceiverDevices.Add (devices.Get (0));
      routerReceiverDevices.Add (devices.Get (1));
    }

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  for (uint32_t i = 0; i < 40; ++i)
    {
      NetDeviceContainer devs;
      devs.Add (senderSwitchDevices.Get (2 * i));
      devs.Add (senderSwitchDevices.Get (2 * i + 1));
      senderSwitchInterfaces.Add (address.Assign (devs));
      address.NewNetwork ();
    }

  address.SetBase ("10.2.1.0", "255.255.255.0");
  switchSwitchInterfaces = address.Assign (switchSwitchDevices);
  address.NewNetwork ();

  address.SetBase ("10.3.1.0", "255.255.255.0");
  for (uint32_t i = 0; i < 20; ++i)
    {
      NetDeviceContainer devs;
      devs.Add (switchRouterDevices.Get (2 * i));
      devs.Add (switchRouterDevices.Get (2 * i + 1));
      switchRouterInterfaces.Add (address.Assign (devs));
      address.NewNetwork ();
    }

  address.SetBase ("10.4.1.0", "255.255.255.0");
  for (uint32_t i = 0; i < 40; ++i)
    {
      NetDeviceContainer devs;
      devs.Add (routerReceiverDevices.Get (2 * i));
      devs.Add (routerReceiverDevices.Get (2 * i + 1));
      routerReceiverInterfaces.Add (address.Assign (devs));
      address.NewNetwork ();
    }

  // Setup Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure RED queues and ECN marking at bottlenecks
  TrafficControlHelper tch;
  QueueDiscTypeId red = TypeId::LookupByName ("ns3::RedQueueDisc");
  tch.SetRootQueueDisc (red);
  tch.SetQueueDiscAttribute ("LinkBandwidth", StringValue ("10Gbps"));
  tch.SetQueueDiscAttribute ("LinkDelay", StringValue ("10us"));
  tch.SetQueueDiscAttribute ("MeanPktSize", StringValue("1500"));
  tch.SetQueueDiscAttribute ("EnablePcn", BooleanValue(true));

  tch.Install (switchSwitchDevices.Get (0));
  tch.Install (switchSwitchDevices.Get (1));
  tch.SetRootQueueDisc (red);
  tch.SetQueueDiscAttribute ("LinkBandwidth", StringValue ("1Gbps"));
  tch.SetQueueDiscAttribute ("LinkDelay", StringValue ("10us"));
  tch.SetQueueDiscAttribute ("MeanPktSize", StringValue("1500"));
  tch.SetQueueDiscAttribute ("EnablePcn", BooleanValue(true));
  tch.Install (switchRouterDevices.Get(1));

  // Create TCP Applications
  uint16_t port = 9;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (routerReceiverInterfaces.GetAddress (0, 0), port));
  source.SetAttribute ("SendSize", UintegerValue (1448));

  ApplicationContainer sources[40];

  for(uint32_t i = 0; i < 40; ++i) {
    BulkSendHelper src ("ns3::TcpSocketFactory", InetSocketAddress (routerReceiverInterfaces.GetAddress (i, 0), port));
    src.SetAttribute ("SendSize", UintegerValue (1448));
    sources[i] = src.Install (senders.Get (i));
    sources[i].Start (Seconds (0.025 * i));
    sources[i].Stop (Seconds (5.0));
  }

  // Sink Application
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinks[40];
  for (uint32_t i = 0; i < 40; i++)
    {
      sinks[i] = sink.Install (receivers.Get (i));
      sinks[i].Start (Seconds (0.0));
      sinks[i].Stop (Seconds (5.0));
    }

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run Simulation
  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();

  // Output Statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalThroughput = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
      totalThroughput += throughput;
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << throughput << " Mbps\n";
    }

  std::cout << "Total aggregate throughput: " << totalThroughput << " Mbps" << std::endl;

  double fairnessIndex = 0.0;
  double sumSquared = 0.0;
  double sum = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());
      sumSquared += throughput * throughput;
      sum += throughput;
    }

  fairnessIndex = (sum * sum) / (40 * sumSquared);

  std::cout << "Fairness Index (Jain's Fairness Index): " << fairnessIndex << std::endl;

  monitor->SerializeToXmlFile("dctcp.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}