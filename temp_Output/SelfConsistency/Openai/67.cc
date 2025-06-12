/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ns-3 Dumbbell Topology Simulation:
 * 7 senders (6 UDP, 1 TCP) -> 1 receiver over a bottleneck link.
 * Runs for two queue discs: COBALT and CoDel.
 * Traces TCP congestion window and queue size for each run.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-disc.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellCobaltCodel");

void
CwndTrace (std::string filename, uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream cwndStream;
  if (!cwndStream.is_open ())
    {
      cwndStream.open (filename, std::ios::out | std::ios::trunc);
      cwndStream << "# Time(s)\tCwnd(Bytes)" << std::endl;
    }
  cwndStream << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
}

void
QueueSizeTrace (std::string filename, uint32_t oldSize, uint32_t newSize)
{
  static std::map<std::string, std::ofstream> streams;
  auto &f = streams[filename];
  if (!f.is_open ())
    {
      f.open (filename, std::ios::out | std::ios::trunc);
      f << "# Time(s)\tQueueSize(Packets)" << std::endl;
    }
  f << Simulator::Now ().GetSeconds () << "\t" << newSize << std::endl;
}

void
experiment (std::string queueDiscType, std::string cwndFile, std::string queueFile, double simTime)
{
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  NodeContainer leftNodes, rightNode, bottleneckNodes;
  leftNodes.Create (7);
  rightNode.Create (1);
  bottleneckNodes.Create (2);

  // Left (senders) --access--> bottleneck[0]
  // bottleneck[1] --access--> right (receiver)
  // bottleneck[0] <--- bottleneck link ---> bottleneck[1]

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (leftNodes);
  stack.Install (rightNode);
  stack.Install (bottleneckNodes);

  // Point-to-point settings
  PointToPointHelper access;
  access.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  access.SetChannelAttribute ("Delay", StringValue ("1ms"));

  PointToPointHelper bottleneck;
  bottleneck.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  bottleneck.SetChannelAttribute ("Delay", StringValue ("20ms"));

  // Connect access links (left 7 senders to bottleneck[0])
  NetDeviceContainer leftDevs, bottleneckLeftDevs;
  for (uint32_t i = 0; i < 7; ++i)
    {
      NetDeviceContainer d = access.Install (leftNodes.Get (i), bottleneckNodes.Get (0));
      leftDevs.Add (d.Get (0));            // sender side interface
      bottleneckLeftDevs.Add (d.Get (1));  // switch side interface
    }

  // Connect access link (bottleneck[1] to right)
  NetDeviceContainer bottleneckRightDev, rightDev;
  NetDeviceContainer d = access.Install (bottleneckNodes.Get (1), rightNode.Get (0));
  bottleneckRightDev.Add (d.Get (0)); // switch side interface
  rightDev.Add (d.Get (1));           // receiver side

  // Connect bottleneck link
  NetDeviceContainer bottleneckDevs = bottleneck.Install (bottleneckNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> leftInterfaces;

  for (uint32_t i = 0; i < 7; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      Ipv4InterfaceContainer iface = address.Assign (NetDeviceContainer (leftDevs.Get (i), bottleneckLeftDevs.Get (i)));
      leftInterfaces.push_back (iface);
    }

  address.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer rightInterface = address.Assign (NetDeviceContainer (bottleneckRightDev.Get (0), rightDev.Get (0)));

  address.SetBase ("10.3.1.0", "255.255.255.0");
  Ipv4InterfaceContainer bottleneckInterface = address.Assign (bottleneckDevs);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install QueueDisc
  TrafficControlHelper tch;
  if (queueDiscType == "cobalt")
    {
      tch.SetRootQueueDisc ("ns3::CobaltQueueDisc");
    }
  else if (queueDiscType == "codel")
    {
      tch.SetRootQueueDisc ("ns3::CoDelQueueDisc");
    }
  else
    {
      NS_ABORT_MSG ("Unknown queueDiscType: " << queueDiscType);
    }

  // Only install on bottleneck device on bottleneckNodes[0] (outgoing to bottleneck[1])
  QueueDiscContainer qdiscs = tch.Install (bottleneckDevs.Get (0));

  // Install applications:
  // * 6 UDP flows: leftNodes 0..5 -> rightNode, unique ports
  // * 1 TCP flow: leftNodes[6]   -> rightNode, port 9000
  uint16_t basePort = 8000;

  // UDP servers (1 per flow)
  ApplicationContainer udpSinks;
  for (uint32_t i = 0; i < 6; ++i)
    {
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                   InetSocketAddress (rightInterface.GetAddress (1), basePort + i));
      udpSinks.Add (sinkHelper.Install (rightNode.Get (0)));
    }

  // TCP sink
  PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory",
                                  InetSocketAddress (rightInterface.GetAddress (1), 9000));
  ApplicationContainer tcpSinkApp = tcpSinkHelper.Install (rightNode.Get (0));

  // UDP clients
  ApplicationContainer udpApps;
  for (uint32_t i = 0; i < 6; ++i)
    {
      UdpClientHelper client (rightInterface.GetAddress (1), basePort + i);
      client.SetAttribute ("MaxPackets", UintegerValue (0));
      client.SetAttribute ("Interval", TimeValue (MilliSeconds (1))); // 1000 packets/sec
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer clientApp = client.Install (leftNodes.Get (i));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (simTime));
      udpApps.Add (clientApp);
    }

  // TCP client
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory",
                             InetSocketAddress (rightInterface.GetAddress (1), 9000));
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer bulkApp = bulkSender.Install (leftNodes.Get (6));
  bulkApp.Start (Seconds (1.0));
  bulkApp.Stop (Seconds (simTime));

  // Start & stop sinks
  udpSinks.Start (Seconds (0.0));
  udpSinks.Stop (Seconds (simTime));
  tcpSinkApp.Start (Seconds (0.0));
  tcpSinkApp.Stop (Seconds (simTime));

  // Congestion window trace for TCP
  Ptr<Socket> tcpSocket = Socket::CreateSocket (leftNodes.Get (6), TcpSocketFactory::GetTypeId ());
  Address sinkAddress = InetSocketAddress (rightInterface.GetAddress (1), 9000);
  tcpSocket->Connect (sinkAddress);

  std::string cwndTraceFile = cwndFile;
  tcpSocket->TraceConnectWithoutContext ("CongestionWindow",
      MakeBoundCallback (&CwndTrace, cwndTraceFile));

  // Queue size trace for bottleneck qdisc
  Ptr<QueueDisc> qdisc = qdiscs.Get (0);
  std::string queueTraceFile = queueFile;
  qdisc->TraceConnectWithoutContext ("PacketsInQueue",
                                    MakeBoundCallback (&QueueSizeTrace, queueTraceFile));

  // Enable pcap if desired (uncomment for debugging)
  // bottleneck.EnablePcapAll("dumbbell");

  // Run Simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  double simTime = 10.0; // seconds

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.Parse (argc, argv);

  // Run for COBALT
  experiment ("cobalt", "tcp_cwnd_cobalt.txt", "queue_size_cobalt.txt", simTime);

  // Run for CoDel
  experiment ("codel", "tcp_cwnd_codel.txt", "queue_size_codel.txt", simTime);

  return 0;
}