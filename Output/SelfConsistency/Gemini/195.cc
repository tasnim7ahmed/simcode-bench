#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpBulkTransfer");

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponent::Enable ("TcpBulkTransfer", LOG_LEVEL_INFO);

  // Allow command-line arguments to configure the simulation.
  CommandLine cmd;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  double simulationTime = 10.0;
  cmd.AddValue ("dataRate", "The data rate for the point-to-point link", dataRate);
  cmd.AddValue ("delay", "The delay for the point-to-point link", delay);
  cmd.AddValue ("simulationTime", "The simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Create point-to-point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  // Install the point-to-point link
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install the internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create BulkSendApplication
  uint16_t port = 9;  // well-known echo port number
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (simulationTime + 1));

  // Create the sink application
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime + 1));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll ("tcp-bulk-transfer");

  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Start simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.bytesSent << "\n";
      std::cout << "  Rx Bytes:   " << i->second.bytesReceived << "\n";
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
    }

  Simulator::Destroy ();

  return 0;
}