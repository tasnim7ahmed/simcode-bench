#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPacingSimulation");

class TcpMetricsTracer {
public:
  void TraceCwnd (Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newVal << std::endl;
  }

  void TracePacingRate (Ptr<OutputStreamWrapper> stream, DataRate oldVal, DataRate newVal) {
    *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newVal.GetBitRate () << std::endl;
  }

  void TraceRtt (Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal) {
    *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newVal.GetSeconds () << std::endl;
  }
};

int main (int argc, char *argv[]) {
  bool enablePacing = true;
  double simTime = 10.0;
  uint32_t initialCWnd = 10;
  std::string transportProt = "TcpNewReno";
  std::string prefixFileName = "tcp-pacing";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enablePacing", "Enable TCP pacing", enablePacing);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("initialCWnd", "Initial congestion window size", initialCWnd);
  cmd.AddValue ("transportProt", "Transport protocol to use: TcpNewReno, TcpBbr, etc.", transportProt);
  cmd.AddValue ("prefix", "Prefix for output filenames", prefixFileName);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 20));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 20));
  Config::SetDefault ("ns3::TcpSocketBase::EnablePacing", BooleanValue (enablePacing));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initialCWnd));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (transportProt)));

  NodeContainer nodes;
  nodes.Create (6);

  PointToPointHelper p2p;

  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer dev_n0n1 = p2p.Install (nodes.Get (0), nodes.Get (1));

  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer dev_n1n2 = p2p.Install (nodes.Get (1), nodes.Get (2));

  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps")); // Bottleneck
  p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));
  NetDeviceContainer dev_n2n3 = p2p.Install (nodes.Get (2), nodes.Get (3));

  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer dev_n3n4 = p2p.Install (nodes.Get (3), nodes.Get (4));

  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer dev_n4n5 = p2p.Install (nodes.Get (4), nodes.Get (5));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0n1 = address.Assign (dev_n0n1);

  address.NewNetwork ();
  Ipv4InterfaceContainer if_n1n2 = address.Assign (dev_n1n2);

  address.NewNetwork ();
  Ipv4InterfaceContainer if_n2n3 = address.Assign (dev_n2n3);

  address.NewNetwork ();
  Ipv4InterfaceContainer if_n3n4 = address.Assign (dev_n3n4);

  address.NewNetwork ();
  Ipv4InterfaceContainer if_n4n5 = address.Assign (dev_n4n5);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;

  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (5));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  AddressValue remoteAddress (InetSocketAddress (if_n4n5.GetAddress (1), port));
  Config::SetDefault ("ns3::TcpSocketFactory::Remote", remoteAddress);

  OnOffHelper onoff ("ns3::TcpSocketFactory", Address ());
  onoff.SetConstantRate (DataRate ("500Kbps"));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer srcApp = onoff.Install (nodes.Get (0));
  srcApp.Start (Seconds (0.1));
  srcApp.Stop (Seconds (simTime - 0.1));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream (prefixFileName + "-cwnd.data");
  Ptr<OutputStreamWrapper> pacingStream = asciiTraceHelper.CreateFileStream (prefixFileName + "-pacing.data");
  Ptr<OutputStreamWrapper> rttStream = asciiTraceHelper.CreateFileStream (prefixFileName + "-rtt.data");

  TcpMetricsTracer tracer;

  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&TcpMetricsTracer::TraceRtt, &tracer));

  Config::Connect ("/NodeList/*/TcpL4Protocol/SocketList/*/$ns3::TcpSocketBase/CongestionWindow", MakeBoundCallback (&TcpMetricsTracer::TraceCwnd, cwndStream));
  Config::Connect ("/NodeList/*/TcpL4Protocol/SocketList/*/$ns3::TcpSocketBase/PacingRate", MakeBoundCallback (&TcpMetricsTracer::TracePacingRate, pacingStream));
  Config::Connect ("/NodeList/*/TcpL4Protocol/SocketList/*/$ns3::TcpSocketBase/RTT", MakeBoundCallback (&TcpMetricsTracer::TraceRtt, rttStream));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
  }

  Simulator::Destroy ();

  return 0;
}