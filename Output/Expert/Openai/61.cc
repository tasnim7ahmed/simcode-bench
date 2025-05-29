#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

void
CwndTracer(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream cwndFile("results/cwndTraces");
  cwndFile << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
QueueSizeTracer(uint32_t oldValue, uint32_t newValue)
{
  static std::ofstream qFile("results/queue-size.dat");
  qFile << Simulator::Now().GetSeconds() << "\t" << newValue << std::endl;
}

void
QueueDropTracer(Ptr<const QueueDiscItem> item)
{
  static std::ofstream dropFile("results/queueTraces");
  dropFile << Simulator::Now().GetSeconds() << "\t" << item->GetPacket()->GetUid() << std::endl;
}

int
main(int argc, char *argv[])
{
  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);

  // Set up point-to-point links
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

  PointToPointHelper p2p1, p2p2, p2p3;
  p2p1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p1.SetChannelAttribute("Delay", StringValue("1ms"));

  p2p2.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p2.SetChannelAttribute("Delay", StringValue("10ms"));

  p2p3.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p3.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer dev01 = p2p1.Install(n0n1);
  NetDeviceContainer dev12 = p2p2.Install(n1n2);
  NetDeviceContainer dev23 = p2p3.Install(n2n3);

  // Enable PCAP tracing
  p2p1.EnablePcapAll("results/pcap/n0n1");
  p2p2.EnablePcapAll("results/pcap/n1n2");
  p2p3.EnablePcapAll("results/pcap/n2n3");

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address1, address2, address3;
  address1.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface01 = address1.Assign(dev01);

  address2.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface12 = address2.Assign(dev12);

  address3.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface23 = address3.Assign(dev23);

  // Set up queue discipline on bottleneck link
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
  Ptr<QueueDisc> q = tch.Install(dev12.Get(0)).Get(0);

  // Schedule stats writing for queues
  Simulator::Schedule(Seconds(0.1), [&](){
    static std::ofstream stats("results/queueStats.txt");
    stats << "Time: " << Simulator::Now().GetSeconds()
          << "s, Queue size: " << q->GetNPackets() << std::endl;
    if (Simulator::Now().GetSeconds() < 9.9) {
      Simulator::Schedule(Seconds(0.1), [&](){
        static std::ofstream stats2("results/queueStats.txt", std::ios_base::app);
        stats2 << "Time: " << Simulator::Now().GetSeconds()
               << "s, Queue size: " << q->GetNPackets() << std::endl;
      });
    }
  });

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // BulkSend application on n0 -> n3
  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(iface23.GetAddress(1), port));

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
  bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApp = bulkSend.Install(nodes.Get(0));
  sourceApp.Start(Seconds(0.5));
  sourceApp.Stop(Seconds(10.0));

  // Trace congestion window
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow",
                                           MakeCallback(&CwndTracer));
  bulkSend.SetAttribute("Socket", PointerValue(ns3TcpSocket));

  // Trace bottleneck queue size and drops
  q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueSizeTracer));
  q->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDropTracer));

  // Record configuration details
  std::ofstream config("results/config.txt");
  config << "Topology:\n"
         << "n0 --- (10Mbps,1ms) --- n1 --- (1Mbps,10ms) --- n2 --- (10Mbps,1ms) --- n3\n";
  config << "TCP BulkSend from n0 to n3\n";
  config << "Queue Discipline: PfifoFast, link n1-n2\n";
  config.close();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}