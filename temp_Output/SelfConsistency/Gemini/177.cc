#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpCongestionExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogFlag (LOG_PREFIX_TIME);

  NodeContainer nodes;
  nodes.Create (3);

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper pointToPointBottleneck;
  pointToPointBottleneck.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPointBottleneck.SetChannelAttribute ("Delay", StringValue ("20ms"));

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer bottleneckDevices;
  bottleneckDevices = pointToPointBottleneck.Install (nodes.Get (0), nodes.Get (1));

  NetDeviceContainer leftDevices;
  leftDevices = pointToPointLeaf.Install (nodes.Get (0), nodes.Get (2));

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer bottleneckInterfaces;
  bottleneckInterfaces = address.Assign (bottleneckDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer leftInterfaces;
  leftInterfaces = address.Assign (leftDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // TCP Reno flow from node 2 to node 1
  uint16_t port = 50000;
  TcpEchoClientHelper clientHelper (leftInterfaces.GetAddress (1), port);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (0)); // Unlimited packets
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));

  TcpEchoServerHelper serverHelper (port);
  ApplicationContainer serverApps = serverHelper.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (2));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // TCP Cubic flow from node 2 to node 1
  // Change the TCP type for this flow to Cubic
  TypeId tid = TypeId::LookupByName ("ns3::TcpCubic");
  GlobalValue::Bind ("Tcp::SocketType", TypeIdValue (tid));

  port = 50001;
  TcpEchoClientHelper clientHelper2 (leftInterfaces.GetAddress (1), port);
  clientHelper2.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper2.SetAttribute ("MaxPackets", UintegerValue (0)); // Unlimited packets
  clientHelper2.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  clientHelper2.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));

  TcpEchoServerHelper serverHelper2 (port);
  ApplicationContainer serverApps2 = serverHelper2.Install (nodes.Get (1));
  serverApps2.Start (Seconds (0.0));
  serverApps2.Stop (Seconds (10.0));

  ApplicationContainer clientApps2 = clientHelper2.Install (nodes.Get (2));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  // Reset TCP type to default (Reno) for future simulations if any
  tid = TypeId::LookupByName ("ns3::TcpReno");
  GlobalValue::Bind ("Tcp::SocketType", TypeIdValue (tid));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  // Add animation interface
  AnimationInterface anim ("tcp-congestion-example.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 20);
  anim.SetConstantPosition (nodes.Get (1), 50, 20);
  anim.SetConstantPosition (nodes.Get (2), 10, 40);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

  monitor->SerializeToXmlFile("tcp-congestion-example.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}