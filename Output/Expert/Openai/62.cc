#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iomanip>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPacingSimulation");

std::ofstream cwndStream, pacingStream, txStream, rxStream;

void
CwndTracer (uint32_t oldCwnd, uint32_t newCwnd)
{
  double t = Simulator::Now ().GetSeconds ();
  cwndStream << std::fixed << std::setprecision (6)
             << t << "," << newCwnd << std::endl;
}

void
PacingTracer (double paceRate)
{
  double t = Simulator::Now ().GetSeconds ();
  pacingStream << std::fixed << std::setprecision (6)
               << t << "," << paceRate << std::endl;
}

void
TxTracer (Ptr<const Packet> packet)
{
  double t = Simulator::Now ().GetSeconds ();
  txStream << std::fixed << std::setprecision (6)
           << t << "," << packet->GetSize () << std::endl;
}

void
RxTracer (Ptr<const Packet> packet, const Address &address)
{
  double t = Simulator::Now ().GetSeconds ();
  rxStream << std::fixed << std::setprecision (6)
           << t << "," << packet->GetSize () << std::endl;
}

void
SetupTracing (Ptr<Socket> sock)
{
  sock->TraceConnectWithoutContext (
      "CongestionWindow", MakeCallback (&CwndTracer));

  Ptr<Object> base = sock;
  if (sock->GetInstanceTypeId () == TypeId::LookupByName ("ns3::TcpSocketBase"))
    {
      Ptr<Object> obj =
          DynamicCast<Object> (sock);
      if (obj)
        {
          obj->TraceConnectWithoutContext (
              "PacingRate", MakeCallback (&PacingTracer));
        }
    }
}

int
main (int argc, char *argv[])
{
  bool pacing = true;
  std::string dataRate1 = "10Mbps", delay1 = "1ms";
  std::string dataRate2 = "2Mbps", delay2 = "50ms"; // bottleneck
  std::string dataRate3 = "10Mbps", delay3 = "1ms";
  uint32_t maxBytes = 0;
  uint32_t initialCwnd = 10;
  double simTime = 10.0;
  std::string tcpType = "ns3::TcpBbr"; // you can set it via CLI
  std::string cwndFile = "cwnd.csv";
  std::string pacingFile = "pacing.csv";
  std::string txFile = "tx.csv";
  std::string rxFile = "rx.csv";

  CommandLine cmd;
  cmd.AddValue ("EnablePacing", "Enable TCP pacing on sockets", pacing);
  cmd.AddValue ("Bandwidth1", "Bandwidth for link n0-n1 and n4-n5", dataRate1);
  cmd.AddValue ("Delay1", "Delay for link n0-n1 and n4-n5", delay1);
  cmd.AddValue ("Bandwidth2", "Bandwidth of bottleneck link n2-n3", dataRate2);
  cmd.AddValue ("Delay2", "Delay for bottleneck link n2-n3", delay2);
  cmd.AddValue ("Bandwidth3", "Bandwidth for link n1-n2 and n3-n4", dataRate3);
  cmd.AddValue ("Delay3", "Delay for link n1-n2 and n3-n4", delay3);
  cmd.AddValue ("InitialCwnd", "Initial congestion window (in segments)", initialCwnd);
  cmd.AddValue ("MaxBytes", "Max bytes the sender will send", maxBytes);
  cmd.AddValue ("SimTime", "Simulation time (seconds)", simTime);
  cmd.AddValue ("TcpType", "TCP variant", tcpType);
  cmd.AddValue ("CwndFile", "Output file for congestion window", cwndFile);
  cmd.AddValue ("PacingFile", "Output file for pacing rate", pacingFile);
  cmd.AddValue ("TxFile", "Output file for packet transmission", txFile);
  cmd.AddValue ("RxFile", "Output file for packet reception", rxFile);
  cmd.Parse (argc, argv);

  cwndStream.open (cwndFile, std::ofstream::out);
  pacingStream.open (pacingFile, std::ofstream::out);
  txStream.open (txFile, std::ofstream::out);
  rxStream.open (rxFile, std::ofstream::out);

  NodeContainer nodes;
  nodes.Create (6);

  // n0--n1--n2==n3--n4--n5
  PointToPointHelper p2p1, p2p2, p2p3;
  // n0-n1, n4-n5
  p2p1.SetDeviceAttribute ("DataRate", StringValue (dataRate1));
  p2p1.SetChannelAttribute ("Delay", StringValue (delay1));
  // n1-n2, n3-n4
  p2p3.SetDeviceAttribute ("DataRate", StringValue (dataRate3));
  p2p3.SetChannelAttribute ("Delay", StringValue (delay3));
  // bottleneck n2-n3
  p2p2.SetDeviceAttribute ("DataRate", StringValue (dataRate2));
  p2p2.SetChannelAttribute ("Delay", StringValue (delay2));

  NetDeviceContainer d0n1 = p2p1.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d1n2 = p2p3.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d2n3 = p2p2.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer d3n4 = p2p3.Install (nodes.Get (3), nodes.Get (4));
  NetDeviceContainer d4n5 = p2p1.Install (nodes.Get (4), nodes.Get (5));

  InternetStackHelper stack;
  Ipv4AddressHelper address;
  stack.Install (nodes);

  Ipv4InterfaceContainer i0n1, i1n2, i2n3, i3n4, i4n5;
  address.SetBase ("10.0.1.0", "255.255.255.0");
  i0n1 = address.Assign (d0n1);
  address.SetBase ("10.0.2.0", "255.255.255.0");
  i1n2 = address.Assign (d1n2);
  address.SetBase ("10.0.3.0", "255.255.255.0");
  i2n3 = address.Assign (d2n3);
  address.SetBase ("10.0.4.0", "255.255.255.0");
  i3n4 = address.Assign (d3n4);
  address.SetBase ("10.0.5.0", "255.255.255.0");
  i4n5 = address.Assign (d4n5);

  // Set TCP version and pacing attribute globally
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpType));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initialCwnd));
  if (pacing)
    {
      Config::SetDefault ("ns3::TcpSocketBase::EnablePacing", BooleanValue (true));
    }
  else
    {
      Config::SetDefault ("ns3::TcpSocketBase::EnablePacing", BooleanValue (false));
    }
  // SACK and segment size for realism
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));

  // (Optional) queue/traffic control realism: drop tail for all links
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue (QueueSize ("100p")));
  tch.Install (d0n1);
  tch.Install (d1n2);
  tch.Install (d2n3);
  tch.Install (d3n4);
  tch.Install (d4n5);

  // TCP Flow: n2 (client) -> n3 (server)
  uint16_t port = 50000;
  Address serverAddr (InetSocketAddress (i2n3.GetAddress (1), port));

  // Receiver/sink on n3
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime + 1));

  // Bulk sender on n2
  BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", serverAddr);
  sourceHelper.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (2));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (simTime));

  // Tracing
  Simulator::Schedule (Seconds (1.0), [&](){
    Ptr<Node> n2 = nodes.Get (2);
    Ptr<Socket> tcpSocket = Socket::CreateSocket (n2, TcpSocketFactory::GetTypeId ());
    tcpSocket->Bind ();
    tcpSocket->Connect (InetSocketAddress (i2n3.GetAddress (1), port));
    SetupTracing (tcpSocket);
  });

  // Standard PCAP tracing
  p2p2.EnablePcapAll ("bottleneck");

  // Application packet transmission tracing
  Ptr<Application> app = sourceApp.Get (0);
  if (app)
    {
      Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (app);
      if (bulk)
        {
          bulk->TraceConnectWithoutContext ("Tx", MakeCallback (&TxTracer));
        }
    }
  // Reception (Rx) tracing
  Ptr<Application> sink = sinkApp.Get (0);
  if (sink)
    {
      Ptr<PacketSink> psink = DynamicCast<PacketSink> (sink);
      if (psink)
        {
          psink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTracer));
        }
    }

  // FlowMonitor for end summary stats
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime + 1));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (auto &i : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i.first);
      if (t.destinationPort == port)
        {
          std::cout << "\nFlow " << i.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i.second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i.second.rxPackets << "\n";
          std::cout << "  Throughput: " << (i.second.rxBytes * 8.0 / (simTime * 1000000)) << " Mbps\n";
          std::cout << "  Lost Packets: " << i.second.lostPackets << "\n";
          std::cout << "  Delay (ms): " << (i.second.delaySum.GetSeconds () * 1000.0 / i.second.rxPackets) << std::endl;
        }
    }

  cwndStream.close ();
  pacingStream.close ();
  txStream.close ();
  rxStream.close ();

  Simulator::Destroy ();
  return 0;
}