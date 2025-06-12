#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RedQueueExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("0.01"));

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install a RedQueueDisc queue on the bottleneck link
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                         "LinkBandwidth", StringValue ("5Mbps"),
                         "LinkDelay", StringValue ("2ms"));
  QueueDiscContainer queueDiscs = tch.Install (devices.Get (1));

  // Create a sink application on node one
  uint16_t port = 9;
  UdpServerHelper sink (port);
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Create a source application on node zero
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (0.01);
  uint32_t maxPacketCount = 1000000;
  UdpClientHelper source (interfaces.GetAddress (1), port);
  source.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  source.SetAttribute ("Interval", TimeValue (interPacketInterval));
  source.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  ApplicationContainer sourceApp = source.Install (nodes.Get (0));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (10.0));

  // Set up a flow monitor to gather statistics
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Print flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
      std::cout << "  Mean delay: " << i->second.delaySum.GetSeconds () / i->second.rxPackets << " s\n";
      std::cout << "  Mean jitter: " << i->second.jitterSum.GetSeconds () / (i->second.rxPackets - 1) << " s\n";
    }

  // Cleanup
  Simulator::Destroy ();

  return 0;
}