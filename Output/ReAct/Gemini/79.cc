#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PointToPointExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t maxBytes = 0;
  std::string animFile = "routing-animation.xml";
  std::string queueDisc = "ns3::FifoQueueDisc";

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("animationFile", "File Name for Animation Output", animFile);
  cmd.AddValue ("maxBytes", "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("queueDiscType", "QueueDisc Type", queueDisc);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (4);

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices02;
  devices02 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));

  NetDeviceContainer devices12;
  devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("100ms"));
  NetDeviceContainer devices13;
  devices13 = pointToPoint.Install (nodes.Get (1), nodes.Get (3));

  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
    NetDeviceContainer devices23;
  devices23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));


  NS_LOG_INFO ("Install Internet Stack.");
  InternetStackHelper internet;
  internet.Install (nodes);

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign (devices02);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (devices12);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i13 = ipv4.Assign (devices13);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set link cost between n1 and n3
  Ipv4GlobalRoutingHelper::SetMetric (i13.GetAddress (0), i13.GetAddress (1), 20);


  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (i12.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (nodes.Get (3));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  // Tracing
  pointToPoint.EnablePcapAll ("routing");

  // Add queue tracing
  // pointToPoint.EnableAsciiAll ("routing");

  // Animation
  AnimationInterface anim (animFile);
  anim.SetConstantPosition (nodes.Get (0), 10, 10);
  anim.SetConstantPosition (nodes.Get (1), 20, 20);
  anim.SetConstantPosition (nodes.Get (2), 30, 10);
  anim.SetConstantPosition (nodes.Get (3), 40, 20);

   FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("routing-flowmon.xml", true, true);

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}