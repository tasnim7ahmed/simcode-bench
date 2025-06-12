/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * TCP Pacing Scenario in NS-3 with Realistic Six-Node Topology
 * - Bottleneck link from n2 to n3 with a TCP flow crossing it
 * - Varying bandwidths & delays
 * - Pacing can be enabled/disabled via CLI
 * - Tracing of CWND, pacing rate, and Tx/Rx times
 * - Command-line adjustable congestion window, pacing, and sim time
 * - Flow Monitor stats at the end
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPacingRealisticTopology");

void
CwndTracer (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

void
PacingRateTracer (Ptr<OutputStreamWrapper> stream, DataRate oldRate, DataRate newRate)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newRate.GetBitRate () << std::endl;
}

void
TxTrace (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << std::endl;
}

void
RxTrace (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &addr)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  // Simulation parameters with defaults
  double simTime = 10.0;              // seconds
  std::string bandwidthA = "100Mbps"; // n0<->n1 and n4<->n5
  std::string delayA = "2ms";
  std::string bandwidthB = "10Mbps";  // n1<->n2 and n3<->n4
  std::string delayB = "5ms";
  std::string bottleneckBW = "2Mbps"; // n2<->n3
  std::string bottleneckDelay = "40ms";
  bool pacing = true;
  uint32_t initialCwnd = 10; // in MSS
  std::string tcpTypeId = "ns3::TcpCubic";
  uint32_t packetSize = 1448; // bytes, typical MSS
  std::string dataRate = "1.5Mbps"; 
  uint32_t maxBytes = 0; // 0 means unlimited

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("pacing", "Enable TCP pacing", pacing);
  cmd.AddValue ("initialCwnd", "Initial TCP congestion window in MSS", initialCwnd);
  cmd.AddValue ("tcpType", "TCP variant", tcpTypeId);
  cmd.AddValue ("packetSize", "Application packet size in bytes", packetSize);
  cmd.AddValue ("dataRate", "Application sending rate", dataRate);
  cmd.Parse (argc, argv);

  LogComponentEnable ("TcpPacingRealisticTopology", LOG_LEVEL_INFO);

  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initialCwnd));

  // Set pacing
  if (pacing)
    {
      Config::SetDefault ("ns3::TcpSocketBase::EnablePacing", BooleanValue (true));
      Config::SetDefault ("ns3::TcpSocketBase::PacingTimePrecision", TimeValue (MilliSeconds (1)));
      Config::SetDefault ("ns3::TcpSocketBase::PacingBurst", UintegerValue (2)); // typical value
    }
  else
    {
      Config::SetDefault ("ns3::TcpSocketBase::EnablePacing", BooleanValue (false));
    }

  // Use specified TCP variant
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpTypeId));

  // Create 6 nodes
  NodeContainer nodes;
  nodes.Create (6);

  // Create topology with the following links:
  // n0---n1---n2===n3---n4---n5
  //        \              /
  //     high BW       high BW
  //     short lat     short lat
  //    low BW/bigRTT bottleneck
  
  // n0 <-> n1
  PointToPointHelper p2pA;
  p2pA.SetDeviceAttribute ("DataRate", StringValue (bandwidthA));
  p2pA.SetChannelAttribute ("Delay", StringValue (delayA));
  NetDeviceContainer d0d1 = p2pA.Install (nodes.Get (0), nodes.Get (1));

  // n1 <-> n2
  PointToPointHelper p2pB;
  p2pB.SetDeviceAttribute ("DataRate", StringValue (bandwidthB));
  p2pB.SetChannelAttribute ("Delay", StringValue (delayB));
  NetDeviceContainer d1d2 = p2pB.Install (nodes.Get (1), nodes.Get (2));

  // n2 <-> n3 (bottleneck!)
  PointToPointHelper p2pBn;
  p2pBn.SetDeviceAttribute ("DataRate", StringValue (bottleneckBW));
  p2pBn.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));
  NetDeviceContainer d2d3 = p2pBn.Install (nodes.Get (2), nodes.Get (3));

  // n3 <-> n4
  NetDeviceContainer d3d4 = p2pB.Install (nodes.Get (3), nodes.Get (4));

  // n4 <-> n5
  NetDeviceContainer d4d5 = p2pA.Install (nodes.Get (4), nodes.Get (5));
  
  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper addr;
  std::vector<Ipv4InterfaceContainer> ifaces;
  
  addr.SetBase ("10.0.0.0", "255.255.255.0");
  ifaces.push_back (addr.Assign (d0d1));
  addr.SetBase ("10.0.1.0", "255.255.255.0");
  ifaces.push_back (addr.Assign (d1d2));
  addr.SetBase ("10.0.2.0", "255.255.255.0");
  ifaces.push_back (addr.Assign (d2d3));
  addr.SetBase ("10.0.3.0", "255.255.255.0");
  ifaces.push_back (addr.Assign (d3d4));
  addr.SetBase ("10.0.4.0", "255.255.255.0");
  ifaces.push_back (addr.Assign (d4d5));

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create a TCP flow from n2 to n3 crossing the bottleneck link
  // For illustration: n2 is sender, n3 is receiver
  uint16_t port = 50000;

  Address sinkAddr (InetSocketAddress (ifaces[2].GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddr);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime + 1));

  // Create a socket on n2, enable pacing as needed
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (2), TcpSocketFactory::GetTypeId ());

  // Set up tracing output files
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream ("cwnd-trace.dat");
  Ptr<OutputStreamWrapper> pacingStream = asciiTraceHelper.CreateFileStream ("pacing-rate-trace.dat");
  Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream ("tx-time-trace.dat");
  Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream ("rx-time-trace.dat");

  // Trace congestion window
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndTracer, cwndStream));

  // Trace pacing rate if enabled
  if (pacing)
    {
      ns3TcpSocket->TraceConnectWithoutContext ("PacingRate", MakeBoundCallback (&PacingRateTracer, pacingStream));
    }

  // Trace packet transmissions from the application
  // Set up BulkSendApplication
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", sinkAddr);
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  bulkSender.SetAttribute ("SendSize", UintegerValue (packetSize));
  bulkSender.SetAttribute ("Socket", PointerValue (ns3TcpSocket));
  ApplicationContainer senderApp = bulkSender.Install (nodes.Get (2));
  senderApp.Start (Seconds (0.5));
  senderApp.Stop (Seconds (simTime + 1));

  // Attach Tx trace at the IP layer of node 2
  nodes.Get (2)->GetDevice (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeBoundCallback (&TxTrace, txStream));

  // Attach Rx trace at the application layer of node 3 (sink)
  sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&RxTrace, rxStream));

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  NS_LOG_INFO ("Starting Simulation.");
  Simulator::Stop (Seconds (simTime + 1));
  Simulator::Run ();

  // Print flow stats
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  std::cout << "\n#### FLOW MONITOR RESULTS ####\n";
  for (const auto& itr : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (itr.first);
      std::cout << "FlowID: " << itr.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << itr.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << itr.second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << itr.second.lostPackets << "\n";
      std::cout << "  Throughput: "
                << (itr.second.rxBytes * 8.0/(simTime * 1000000.0)) << " Mbps\n";
      std::cout << "  Mean delay: "
                << (itr.second.delaySum.GetSeconds () / (double) itr.second.rxPackets)
                << " s\n";
    }

  Simulator::Destroy ();

  NS_LOG_INFO ("Simulation completed and trace files generated.");
  return 0;
}