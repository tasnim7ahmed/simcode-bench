#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPacingExample");

std::ofstream cwndStream;
std::ofstream pacingStream;
std::ofstream txStream;
std::ofstream rxStream;

void
CwndTracer (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  cwndStream << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
}

void
TxTracer (std::string context, Ptr<const Packet> p, const TcpHeader& h, Ptr<const TcpSocketBase>)
{
  txStream << Simulator::Now ().GetSeconds ()
           << "\t" << h.GetSequenceNumber ().GetValue ()
           << "\t" << p->GetSize () << std::endl;
}

void
RxTracer (Ptr<const Packet> p, const Address &address)
{
  rxStream << Simulator::Now ().GetSeconds () << "\t" << p->GetSize () << std::endl;
}

void
PacingRateTracer (std::string context, DataRate oldRate, DataRate newRate)
{
  pacingStream << Simulator::Now ().GetSeconds () << "\t"
               << newRate.GetBitRate () << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  bool pacing = true;
  std::string tcpVariant = "ns3::TcpCubic";
  double simulationTime = 10.0;
  std::string bottleneckBw = "8Mbps";
  std::string bottleneckDelay = "50ms";
  uint32_t initCwnd = 10;
  uint32_t maxBytes = 0;

  cmd.AddValue ("EnablePacing", "Enable TCP pacing", pacing);
  cmd.AddValue ("TcpVariant", "TCP variant to use", tcpVariant);
  cmd.AddValue ("SimulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("BottleneckBw", "Bottleneck bandwidth", bottleneckBw);
  cmd.AddValue ("BottleneckDelay", "Bottleneck delay (e.g., 50ms)", bottleneckDelay);
  cmd.AddValue ("InitialCwnd", "Initial congestion window (segments)", initCwnd);
  cmd.AddValue ("MaxBytes", "Specified value to Applications, 0=unlimited", maxBytes);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initCwnd));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpVariant));
  Config::SetDefault ("ns3::TcpSocketBase::EnablePacing", BooleanValue (pacing));
  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue ("output-attributes.txt"));

  NodeContainer nodes;
  nodes.Create (6);

  // Topology inspired by real-world example (n0-n5):
  // n0--n1--n2==n3--n4--n5
  //                ||
  //              bottleneck

  PointToPointHelper p2pA;
  p2pA.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pA.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper p2pB;
  p2pB.SetDeviceAttribute ("DataRate", StringValue (bottleneckBw));
  p2pB.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

  NetDeviceContainer d01 = p2pA.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d12 = p2pA.Install (nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d23 = p2pB.Install (nodes.Get(2), nodes.Get(3));
  NetDeviceContainer d34 = p2pA.Install (nodes.Get(3), nodes.Get(4));
  NetDeviceContainer d45 = p2pA.Install (nodes.Get(4), nodes.Get(5));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i34 = ipv4.Assign (d34);
  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i45 = ipv4.Assign (d45);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // TCP flow from n2 to n3 crossing the bottleneck
  uint16_t servPort = 50000;

  Address serverAddr (InetSocketAddress (i34.GetAddress (0), servPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), servPort));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 2));

  OnOffHelper onoff ("ns3::TcpSocketFactory", serverAddr);
  onoff.SetAttribute ("DataRate", StringValue ("50Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1448));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime + 1.0)));
  if (maxBytes > 0)
    {
      onoff.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    }

  ApplicationContainer clientApp = onoff.Install (nodes.Get (2));

  // Tracing
  cwndStream.open ("cwnd.txt");
  pacingStream.open ("pacing.txt");
  txStream.open ("tx.txt");
  rxStream.open ("rx.txt");

  // Find the TCP socket
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get(2), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->Bind ();

  // Manually connect traces (for main flow)
  Config::Connect ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));
  Config::Connect ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/PacingRate", MakeCallback (&PacingRateTracer));
  Config::Connect ("/NodeList/2/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&RxTracer));
  Config::Connect ("/NodeList/2/ApplicationList/*/$ns3::OnOffApplication/Tx", MakeCallback (&TxTracer));

  // In case sockets are created by the app after simulation start
  Config::Connect ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeCallback (&CwndTracer));
  Config::Connect ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/*/PacingRate", MakeCallback (&PacingRateTracer));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 2));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  std::cout << "------ Flow Monitor Results -------" << std::endl;
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      std::cout << "FlowID: " << iter->first << " [" << t.sourceAddress << " --> " << t.destinationAddress << "]\n";
      std::cout << "  Tx Bytes:   " << iter->second.txBytes << std::endl;
      std::cout << "  Rx Bytes:   " << iter->second.rxBytes << std::endl;
      std::cout << "  Throughput: "
                << iter->second.rxBytes * 8.0 / (simulationTime * 1000000.0)
                << " Mbps\n";
      std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
    }

  cwndStream.close ();
  pacingStream.close ();
  txStream.close ();
  rxStream.close ();

  Simulator::Destroy ();
  return 0;
}