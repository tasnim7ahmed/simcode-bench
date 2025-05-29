#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

void
CwndChangeCallback(std::string path, uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream cwndFile("results/cwndTraces");
  cwndFile << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

void
QueueSizeTrace(std::string context, uint32_t oldValue, uint32_t newValue)
{
  static std::ofstream queueFile("results/queue-size.dat");
  queueFile << Simulator::Now().GetSeconds() << "\t" << newValue << std::endl;
}

void
QueueDropTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream dropFile("results/queueTraces");
  dropFile << Simulator::Now().GetSeconds() << std::endl;
}

int
main(int argc, char *argv[])
{
  // Output file directories
  system("mkdir -p results");
  system("mkdir -p results/pcap");

  // Write config.txt
  std::ofstream config("results/config.txt");
  config << "n0 <-> n1: 10Mbps, 1ms\n";
  config << "n1 <-> n2: 1Mbps, 10ms\n";
  config << "n2 <-> n3: 10Mbps, 1ms\n";
  config << "TCP BulkSend from n0 to n3\n";
  config.close();

  // Nodes
  NodeContainer nodes;
  nodes.Create(4);

  // Channels and devices
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p1.SetChannelAttribute("Delay", StringValue("1ms"));

  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p2.SetChannelAttribute("Delay", StringValue("10ms"));

  PointToPointHelper p2p3;
  p2p3.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p3.SetChannelAttribute("Delay", StringValue("1ms"));

  // Connect links
  NetDeviceContainer d0d1 = p2p1.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d1d2 = p2p2.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d2d3 = p2p3.Install(nodes.Get(2), nodes.Get(3));

  // Enable PCAP tracing
  p2p1.EnablePcapAll("results/pcap/p2p1", false);
  p2p2.EnablePcapAll("results/pcap/p2p2", false);
  p2p3.EnablePcapAll("results/pcap/p2p3", false);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  address.SetBase("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Applications
  uint16_t port = 5000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  BulkSendHelper bulkSend("ns3::TcpSocketFactory",
      InetSocketAddress(i2i3.GetAddress(1), port));
  bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApp = bulkSend.Install(nodes.Get(0));
  sourceApp.Start(Seconds(0.5));
  sourceApp.Stop(Seconds(10.0));

  // Congestion Window Tracing (on n0 TCP socket)
  Config::Connect(
    "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
    MakeCallback(&CwndChangeCallback));

  // Find the queue on n1->n2 link (devices: d1d2)
  Ptr<PointToPointNetDevice> nd1 = DynamicCast<PointToPointNetDevice>(d1d2.Get(0));
  Ptr<Queue<Packet> > queue = nd1->GetQueue();
  if (queue)
  {
    queue->TraceConnect("PacketsInQueue", "", MakeCallback(&QueueSizeTrace));
    queue->TraceConnect("Drop", "", MakeCallback(&QueueDropTrace));
  }

  // Simulation run and collect final queue stats
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Write queue stats to file
  std::ofstream queueStats("results/queueStats.txt");
  if (queue)
  {
    queueStats << "Current queue size: " << queue->GetNPackets() << std::endl;
    queueStats << "Max queue size: ";
    Ptr<DropTailQueue<Packet> > dtq = DynamicCast<DropTailQueue<Packet> >(queue);
    if (dtq)
      queueStats << dtq->GetMaxPackets() << std::endl;
    else
      queueStats << "unknown" << std::endl;
  }
  queueStats.close();
  Simulator::Destroy();
  return 0;
}