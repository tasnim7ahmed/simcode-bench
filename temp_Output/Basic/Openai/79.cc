#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FourNodeRerouteExample");

void RxTrace (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &address)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << std::endl;
}

int main (int argc, char *argv[])
{
  uint32_t nNodes = 4;
  std::string n1n3Metric = "10";
  std::string traceFileName = "simulation-trace.log";
  std::string queueTraceFile = "queue-trace.log";
  std::string rxTraceFile = "rx-trace.log";

  CommandLine cmd;
  cmd.AddValue ("n1n3Metric", "Set the metric for the n1-n3 link", n1n3Metric);
  cmd.AddValue ("traceFileName", "Simulation log file", traceFileName);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (nNodes);
  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n1 = nodes.Get(1);
  Ptr<Node> n2 = nodes.Get(2);
  Ptr<Node> n3 = nodes.Get(3);

  // n0 - n2
  NodeContainer n0n2 (n0, n2);
  // n1 - n2
  NodeContainer n1n2 (n1, n2);
  // n1 - n3
  NodeContainer n1n3 (n1, n3);
  // n2 - n3
  NodeContainer n2n3 (n2, n3);

  // Point-to-point configs
  PointToPointHelper p2p_n0n2;
  p2p_n0n2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_n0n2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_n0n2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  PointToPointHelper p2p_n1n2;
  p2p_n1n2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p_n1n2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_n1n2.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  PointToPointHelper p2p_n1n3;
  p2p_n1n3.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p_n1n3.SetChannelAttribute ("Delay", StringValue ("100ms"));
  p2p_n1n3.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  PointToPointHelper p2p_n2n3;
  p2p_n2n3.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p_n2n3.SetChannelAttribute ("Delay", StringValue ("10ms"));
  p2p_n2n3.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("100p"));

  NetDeviceContainer dev_n0n2 = p2p_n0n2.Install (n0n2);
  NetDeviceContainer dev_n1n2 = p2p_n1n2.Install (n1n2);
  NetDeviceContainer dev_n1n3 = p2p_n1n3.Install (n1n3);
  NetDeviceContainer dev_n2n3 = p2p_n2n3.Install (n2n3);

  // Address assignment
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  // n0-n2
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0n2 = ipv4.Assign (dev_n0n2);

  // n1-n2
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1n2 = ipv4.Assign (dev_n1n2);

  // n1-n3
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n1n3 = ipv4.Assign (dev_n1n3);

  // n2-n3
  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n2n3 = ipv4.Assign (dev_n2n3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set metric on n1-n3 link
  Ptr<Ipv4> ipv4_n1 = n1->GetObject<Ipv4>();
  uint32_t ifIndex_n1n3 = 2; // Based on installation order (typically p2p devices become interfaces 1,2...)
  // To ensure correct, let's find the interface associated with n1-n3
  for (uint32_t i=0; i < ipv4_n1->GetNInterfaces(); ++i)
    {
      for (uint32_t j=0; j < ipv4_n1->GetNAddresses(i); ++j)
        {
          if (ipv4_n1->GetAddress(i, j).GetLocal() == if_n1n3.GetAddress(0))
            {
              ifIndex_n1n3 = i;
              break;
            }
        }
    }
  ipv4_n1->SetMetric (ifIndex_n1n3, std::stoi(n1n3Metric));
  Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();

  // Enable Queue tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> queueStream = ascii.CreateFileStream (queueTraceFile);
  dev_n1n2.Get (0)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&AsciiTraceHelper::DefaultDropSink, queueStream));
  dev_n1n2.Get (1)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&AsciiTraceHelper::DefaultDropSink, queueStream));
  dev_n2n3.Get (0)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&AsciiTraceHelper::DefaultDropSink, queueStream));
  dev_n2n3.Get (1)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&AsciiTraceHelper::DefaultDropSink, queueStream));
  dev_n1n3.Get (0)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&AsciiTraceHelper::DefaultDropSink, queueStream));
  dev_n1n3.Get (1)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&AsciiTraceHelper::DefaultDropSink, queueStream));

  // Rx trace at n1
  Ptr<OutputStreamWrapper> rxStream = ascii.CreateFileStream (rxTraceFile);
  dev_n1n2.Get (0)->TraceConnectWithoutContext ("MacRx", MakeBoundCallback (&RxTrace, rxStream));
  dev_n1n3.Get (0)->TraceConnectWithoutContext ("MacRx", MakeBoundCallback (&RxTrace, rxStream));

  // Install UDP echo server on n1 and a UDP client on n3
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (n1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (12.0));

  UdpEchoClientHelper echoClient (if_n1n2.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.05)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));

  // For n3 to n1 by IP, use the address on n1 for n1-n2 (could try also n1-n3).
  ApplicationContainer clientApps = echoClient.Install (n3);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (12.0));

  // Enable pcap tracing for all devices
  p2p_n0n2.EnablePcapAll ("n0n2");
  p2p_n1n2.EnablePcapAll ("n1n2");
  p2p_n1n3.EnablePcapAll ("n1n3");
  p2p_n2n3.EnablePcapAll ("n2n3");

  // FlowMonitor for statistics
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (13.0));

  Simulator::Run ();

  std::ofstream file (traceFileName, std::ios::out | std::ios::trunc);

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (const auto& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      file << "Flow: " << flow.first << " Src " << t.sourceAddress << " Dst " << t.destinationAddress << std::endl;
      file << "  Tx Packets:   " << flow.second.txPackets << std::endl;
      file << "  Rx Packets:   " << flow.second.rxPackets << std::endl;
      file << "  Throughput: " << (flow.second.rxBytes * 8.0 / 10.0 / 1000.0) << " kbps" << std::endl;
      file << "  Lost Packets: " << flow.second.lostPackets << std::endl;
      file << "  Delay (sum):  " << flow.second.delaySum.GetSeconds () << "s" << std::endl;
      file << "  Jitter (sum): " << flow.second.jitterSum.GetSeconds () << "s" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}