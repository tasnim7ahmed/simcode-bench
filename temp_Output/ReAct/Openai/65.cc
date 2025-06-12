#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ping-helper.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FqCoDelTrafficControlExample");

void
QueueDiscStatsCallback (Ptr<QueueDisc> queue)
{
  static std::ofstream dropStream ("queue-drops.txt", std::ios::out | std::ios::app);
  static std::ofstream markStream ("queue-marks.txt", std::ios::out | std::ios::app);
  static std::ofstream lengthStream ("queue-length.txt", std::ios::out | std::ios::app);

  uint32_t drops = queue->GetStats().nDroppedPackets;
  uint32_t marks = queue->GetStats().nMarkedPackets;
  uint32_t qlen = queue->GetCurrentSize().GetValue();
  double t = Simulator::Now ().GetSeconds ();
  dropStream << t << "\t" << drops << std::endl;
  markStream << t << "\t" << marks << std::endl;
  lengthStream << t << "\t" << qlen << std::endl;
}

void
TraceCwnd (uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream cwndStream ("cwnd.txt", std::ios::out | std::ios::app);
  cwndStream << Simulator::Now().GetSeconds () << "\t" << newCwnd << std::endl;
}

void
TraceRtt (Time oldVal, Time newVal)
{
  static std::ofstream rttStream ("rtt.txt", std::ios::out | std::ios::app);
  rttStream << Simulator::Now().GetSeconds () << "\t" << newVal.GetMilliSeconds () << std::endl;
}

void
TraceThroughput (Ptr<PacketSink> sink)
{
  static uint64_t lastTotalRx = 0;
  static Time lastTime = Seconds (0);
  uint64_t curRx = sink->GetTotalRx ();
  Time curTime = Simulator::Now ();
  double throughput = (curRx - lastTotalRx) * 8.0 / (curTime.GetSeconds () - lastTime.GetSeconds ()) / 1e6;
  static std::ofstream tpStream ("throughput.txt", std::ios::out | std::ios::app);
  tpStream << curTime.GetSeconds () << "\t" << throughput << std::endl;
  lastTotalRx = curRx;
  lastTime = curTime;
  Simulator::Schedule (Seconds (0.1), &TraceThroughput, sink);
}

void
TraceDctcpAlpha (DoubleValue oldVal, DoubleValue newVal)
{
  static std::ofstream alphaStream ("dctcp-alpha.txt", std::ios::out | std::ios::app);
  alphaStream << Simulator::Now().GetSeconds () << "\t" << newVal.Get () << std::endl;
}

// Main
int main(int argc, char *argv[])
{
  uint32_t numNodes = 4;
  double simTime = 10.0;
  bool enablePcap = false;
  bool enableDctcp = false;
  uint32_t maxBytes = 5 * 1024 * 1024;
  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableDctcp", "Enable DCTCP (if supported)", enableDctcp);
  cmd.AddValue ("maxBytes", "BulkSend MaxBytes", maxBytes);
  cmd.AddValue ("simTime", "Simulation duration (s)", simTime);
  cmd.Parse(argc, argv);

  // Create nodes: [client1]---[r1]---[r2]---[server]
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Point-to-point links
  // n0<->n1: access, n1<->n2: bottleneck, n2<->n3: access
  PointToPointHelper p2pAccess, p2pBottleneck;
  p2pAccess.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2pAccess.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pBottleneck.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pBottleneck.SetChannelAttribute ("Delay", StringValue ("20ms"));

  NetDeviceContainer d01 = p2pAccess.Install(nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d12 = p2pBottleneck.Install(nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d23 = p2pAccess.Install(nodes.Get (2), nodes.Get (3));

  // Install Internet stack and traffic control
  InternetStackHelper stack;
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc",
                        "QueueLimits", StringValue ("ns3::DynamicQueueLimits"));

  stack.InstallAll ();
  tch.Install (d12); // Only bottleneck

  // Assign IPv4 addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if01 = ipv4.Assign (d01);
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if12 = ipv4.Assign (d12);
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if23 = ipv4.Assign (d23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // ICMP/Ping: n0 -> n3 periodically
  PingHelper ping (if23.GetAddress (1));
  ping.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping.SetAttribute ("Verbose", BooleanValue (false));
  ApplicationContainer pingApps = ping.Install (nodes.Get (0));
  pingApps.Start (Seconds (0.2));
  pingApps.Stop (Seconds (simTime));

  // TCP BulkSend: n0->n3
  uint16_t tcpPort = 9000;
  Address sinkAddress (InetSocketAddress (if23.GetAddress (1), tcpPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simTime+1.0));

  BulkSendHelper bulkHelper ("ns3::TcpSocketFactory", sinkAddress);
  bulkHelper.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer bulkApps = bulkHelper.Install (nodes.Get (0));
  bulkApps.Start (Seconds (1.0));
  bulkApps.Stop (Seconds (simTime));

  // Queue disc tracing
  Ptr<NetDevice> bottleneckDev = d12.Get(0);
  Ptr<TrafficControlLayer> tc = nodes.Get (1)->GetObject<TrafficControlLayer> ();
  Ptr<QueueDisc> qdisc = tc->GetQueueDiscOnNetDevice (bottleneckDev);
  if (!qdisc)
    {
      std::cerr << "QueueDisc not found on bottleneck device.\n";
      return 1;
    }
  Simulator::Schedule (Seconds (0.1), &QueueDiscStatsCallback, qdisc);

  // Traces: RTT, CWND, Throughput
  Ptr<Socket> tcpSocket = Socket::CreateSocket (nodes.Get(0), TcpSocketFactory::GetTypeId ());
  if (enableDctcp)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));
    }
  else
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    }
  bulkHelper.SetAttribute ("SendSize", UintegerValue (1448));

  tcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&TraceCwnd));
  tcpSocket->TraceConnectWithoutContext ("RTT", MakeCallback (&TraceRtt));
  if (enableDctcp)
    {
      Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpSocketBase/DcAlpha", MakeCallback (&TraceDctcpAlpha));
    }

  Simulator::Schedule (Seconds (1.1), &TraceThroughput, StaticCast<PacketSink> (sinkApps.Get (0)));

  // PCAP
  if (enablePcap)
    {
      p2pAccess.EnablePcapAll ("access", false);
      p2pBottleneck.EnablePcapAll ("bottleneck", false);
    }

  Simulator::Stop (Seconds(simTime+2.0));
  Simulator::Run ();
  Simulator::Destroy ();

  // Close trace files
  {
    std::ofstream tmp;
    tmp.open ("queue-drops.txt", std::ios_base::app); tmp.close ();
    tmp.open ("queue-marks.txt", std::ios_base::app); tmp.close ();
    tmp.open ("queue-length.txt", std::ios_base::app); tmp.close ();
    tmp.open ("cwnd.txt", std::ios_base::app); tmp.close ();
    tmp.open ("rtt.txt", std::ios_base::app); tmp.close ();
    tmp.open ("throughput.txt", std::ios_base::app); tmp.close ();
    tmp.open ("dctcp-alpha.txt", std::ios_base::app); tmp.close ();
  }
  return 0;
}