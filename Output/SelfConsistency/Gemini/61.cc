#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpExample");

// Callback for tracing the congestion window (cwnd)
static void
CwndTracer (std::string context, uint32_t cwnd)
{
  std::ofstream outfile;
  outfile.open ("results/cwndTraces", std::ios::app);
  outfile << Simulator::Now ().GetSeconds () << " " << cwnd << std::endl;
  outfile.close ();
}

// Callback for tracing the queue length
static void
QueueTracer (std::string context, uint32_t queueSize)
{
  std::ofstream outfile;
  outfile.open ("results/queue-size.dat", std::ios::app);
  outfile << Simulator::Now ().GetSeconds () << " " << queueSize << std::endl;
  outfile.close ();
}

int
main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("TcpExample", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Create point-to-point links
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("1ms"));
  QueueSize queueSize1 = QueueSize ("50p");
  p2p1.SetQueue("ns3::DropTailQueue", "MaxPackets", StringValue("50"));

  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  QueueSize queueSize2 = QueueSize ("50p");
  p2p2.SetQueue("ns3::DropTailQueue", "MaxPackets", StringValue("50"));

  PointToPointHelper p2p3;
  p2p3.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p3.SetChannelAttribute ("Delay", StringValue ("1ms"));
  QueueSize queueSize3 = QueueSize ("50p");
  p2p3.SetQueue("ns3::DropTailQueue", "MaxPackets", StringValue("50"));

  // Create NetDevice containers
  NetDeviceContainer d0d1 = p2p1.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d1d2 = p2p2.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d2d3 = p2p3.Install (nodes.Get (2), nodes.Get (3));

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign (d2d3);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create sink application on node 3
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  // Create bulk send application on node 0
  BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", InetSocketAddress (i2i3.GetAddress (1), port));
  sourceHelper.SetAttribute ("SendSize", UintegerValue (1448));
  ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (0));
  sourceApp.Start (Seconds (2.0));
  sourceApp.Stop (Seconds (10.0));

  // Configure tracing of cwnd
  Config::ConnectWithoutContext ("/ChannelList/0/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));

  // Trace queue size
  // Get queue object
  Ptr<Queue> queue = d1d2.GetDevice(0)->GetQueue();
  // Connect trace source
  Config::ConnectWithoutContext(queue->GetPath() + "/MaxSize", MakeCallback(&QueueTracer));
  Config::ConnectWithoutContext(queue->GetPath() + "/Size", MakeCallback(&QueueTracer));


  // Create PCAP Traces
  p2p1.EnablePcapAll ("results/pcap/tcp-example-0-1");
  p2p2.EnablePcapAll ("results/pcap/tcp-example-1-2");
  p2p3.EnablePcapAll ("results/pcap/tcp-example-2-3");

  //FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create output directory if it doesn't exist
  std::string dir = "results";
  if (mkdir(dir.c_str(), 0777) == -1) {
    if (errno != EEXIST) {
        std::cerr << "Error creating directory " << dir << std::endl;
        exit(1);
    }
  }

  dir = "results/pcap";
  if (mkdir(dir.c_str(), 0777) == -1) {
      if (errno != EEXIST) {
          std::cerr << "Error creating directory " << dir << std::endl;
          exit(1);
      }
  }

  // Write configuration to a file
  std::ofstream configFile;
  configFile.open ("results/config.txt");
  configFile << "Data rate 0->1: 10Mbps\n";
  configFile << "Delay 0->1: 1ms\n";
  configFile << "Data rate 1->2: 1Mbps\n";
  configFile << "Delay 1->2: 10ms\n";
  configFile << "Data rate 2->3: 10Mbps\n";
  configFile << "Delay 2->3: 1ms\n";
  configFile << "TCP flow from n0 to n3\n";
  configFile.close ();

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
	  std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	  std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("results/queueStats.txt", true, true);
  Simulator::Destroy ();

  return 0;
}