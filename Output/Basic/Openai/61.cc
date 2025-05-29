#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

void
CwndTracer(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream cwndFile("results/cwndTraces");
  cwndFile << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

void
QueueSizeTracer(Ptr<QueueDisc> queue)
{
  static std::ofstream queueSizeFile("results/queue-size.dat", std::ios_base::app);
  queueSizeFile << Simulator::Now().GetSeconds() << " " << queue->GetCurrentSize().GetValue() << std::endl;
  Simulator::Schedule(MilliSeconds(1), &QueueSizeTracer, queue);
}

void
QueueDropTracer(Ptr<const QueueDiscItem> item)
{
  static std::ofstream dropFile("results/queueTraces", std::ios_base::app);
  dropFile << Simulator::Now().GetSeconds() << " " << item->GetPacket()->GetSize() << std::endl;
}

void
WriteQueueStats(Ptr<QueueDisc> queue)
{
  std::ofstream qstat("results/queueStats.txt");
  qstat << "Queue disc statistics:\n";
  qstat << "  Packets enqueued: " << queue->GetStats().nTotalReceivedPackets << std::endl;
  qstat << "  Packets dropped: " << queue->GetStats().nTotalDroppedPackets << std::endl;
  qstat << "  Bytes received: " << queue->GetStats().nTotalReceivedBytes << std::endl;
  qstat << "  Bytes dropped: " << queue->GetStats().nTotalDroppedBytes << std::endl;
  qstat.close();
}

int
main(int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                     TypeIdValue(TcpSocketFactory::GetTypeId()));

  // Create results directory
  system("mkdir -p results/pcap");

  NodeContainer nodes;
  nodes.Create(4);

  // n0<->n1 (10 Mbps, 1 ms)
  NodeContainer n0n1(nodes.Get(0), nodes.Get(1));
  PointToPointHelper p2p0;
  p2p0.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p0.SetChannelAttribute("Delay", StringValue("1ms"));

  // n1<->n2 (1 Mbps, 10 ms)
  NodeContainer n1n2(nodes.Get(1), nodes.Get(2));
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p1.SetChannelAttribute("Delay", StringValue("10ms"));

  // n2<->n3 (10 Mbps, 1 ms)
  NodeContainer n2n3(nodes.Get(2), nodes.Get(3));
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p2.SetChannelAttribute("Delay", StringValue("1ms"));

  // NetDevices
  NetDeviceContainer d0 = p2p0.Install(n0n1);
  NetDeviceContainer d1 = p2p1.Install(n1n2);
  NetDeviceContainer d2 = p2p2.Install(n2n3);

  // Traffic Control for bottleneck link (n1<->n2)
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
  Ptr<QueueDisc> queueDisc = tch.Install(d1).Get(0);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0 = ipv4.Assign(d0);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = ipv4.Assign(d1);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign(d2);

  // Enable PCAPs
  p2p0.EnablePcapAll("results/pcap/n0n1");
  p2p1.EnablePcapAll("results/pcap/n1n2");
  p2p2.EnablePcapAll("results/pcap/n2n3");

  // BulkSendApplication on n0
  uint16_t port = 5000;
  BulkSendHelper source("ns3::TcpSocketFactory",
                        InetSocketAddress(i2.GetAddress(1), port));
  source.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApp = source.Install(nodes.Get(0));
  sourceApp.Start(Seconds(0.5));
  sourceApp.Stop(Seconds(10.0));

  // PacketSink on n3
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(3));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(11.0));

  // Trace congestion window
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  ns3TcpSocket->TraceConnect("CongestionWindow", "", MakeCallback(&CwndTracer));

  // Trace queue size and drops on bottleneck
  Simulator::Schedule(MilliSeconds(0), &QueueSizeTracer, queueDisc);
  queueDisc->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDropTracer));

  // Write configuration details
  std::ofstream configFile("results/config.txt");
  configFile << "Topology:\n";
  configFile << " n0 <-> n1 (10Mbps, 1ms) <-> n2 (1Mbps, 10ms) <-> n3 (10Mbps, 1ms)\n";
  configFile << "TCP: " << Config::GetDefaultFailSafe<std::string>("ns3::TcpL4Protocol::SocketType") << "\n";
  configFile << "Application: BulkSend from n0 to n3, MaxBytes=0\n";
  configFile << "Tracing:\n";
  configFile << "  cwnd: results/cwndTraces\n";
  configFile << "  PCAP: results/pcap/\n";
  configFile << "  Queue Size: results/queue-size.dat\n";
  configFile << "  Drop trace: results/queueTraces\n";
  configFile << "  Queue Stats: results/queueStats.txt\n";
  configFile.close();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  // Write out queue stats
  WriteQueueStats(queueDisc);

  Simulator::Destroy();
  return 0;
}