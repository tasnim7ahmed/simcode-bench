#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

// Globals for tracing
static std::ofstream cwndStream;
static std::ofstream throughputStream;
static std::ofstream queueStream;
static uint32_t lastTotalRx = 0;

// Congestion window trace callback
void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

// Throughput trace callback, called every 100ms
void
TraceThroughput(Ptr<Application> app, Ptr<OutputStreamWrapper> stream)
{
  static Time lastTime = Seconds(0.0);
  uint32_t curRx = DynamicCast<BulkSendApplication>(app)->GetTotalBytes();
  double throughput = ((curRx - lastTotalRx) * 8.0) / 1e5; // Mbps over 0.1s
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughput << std::endl;
  lastTotalRx = curRx;
  Simulator::Schedule(MilliSeconds(100), &TraceThroughput, app, stream);
}

// Queue length trace callback
void
QueueSizeTrace(uint32_t oldValue, uint32_t newValue)
{
  queueStream << Simulator::Now().GetSeconds() << "\t" << newValue << std::endl;
}

// BBR PROBE_RTT trigger: forcibly enter PROBE_RTT phase by changing min_rtt and cwnd
void
EnterProbeRtt(Ptr<Socket> sock, uint32_t segmentSize)
{
  // For TCP BBR in ns-3, set socket option to reduce allowed cwnd (simulate PROBE_RTT)
  // The actual BBR implementation doesn't expose direct phase manipulation,
  // but reducing allowed congestion window will have a similar effect.
  // Set the socket's congestion window to 4 MSS temporarily
  sock->SetAttribute("CongestionWindow", UintegerValue(4 * segmentSize));
  Simulator::Schedule(Seconds(0.2), [sock, segmentSize]() {
    sock->SetAttribute("CongestionWindow", UintegerValue(1000000)); // Restore to very large value (let BBR control)
  });
  // Schedule the next PROBE_RTT
  Simulator::Schedule(Seconds(10.0), &EnterProbeRtt, sock, segmentSize);
}

int
main(int argc, char* argv[])
{
  // Output files
  std::string cwndFile = "cwnd-trace.txt";
  std::string throughputFile = "throughput-trace.txt";
  std::string queueFile = "queue-size-trace.txt";

  // Open trace file streams
  cwndStream.open(cwndFile);
  throughputStream.open(throughputFile);
  queueStream.open(queueFile);

  // Create nodes: 0=Sender, 1=R1, 2=R2, 3=Receiver
  NodeContainer nodes;
  nodes.Create(4);

  // Links containers
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1)); // Sender-R1
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2)); // R1-R2 (bottleneck)
  NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3)); // R2-Receiver

  // Point-to-point helpers
  PointToPointHelper p2p_top, p2p_bottleneck, p2p_bottom;
  p2p_top.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
  p2p_top.SetChannelAttribute("Delay", StringValue("5ms"));

  p2p_bottleneck.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p_bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
  // Use a DropTail queue with depth 100 packets on the bottleneck
  p2p_bottleneck.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

  p2p_bottom.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
  p2p_bottom.SetChannelAttribute("Delay", StringValue("5ms"));

  // Install devices
  NetDeviceContainer d0d1 = p2p_top.Install(n0n1);
  NetDeviceContainer d1d2 = p2p_bottleneck.Install(n1n2);
  NetDeviceContainer d2d3 = p2p_bottom.Install(n2n3);

  // Install the Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Set BBR as default TCP congestion control
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpBbr")));

  // Application: BulkSend from sender to receiver
  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(100.0));

  BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
  sourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited
  ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
  sourceApp.Start(Seconds(1.0));
  sourceApp.Stop(Seconds(100.0));

  // Pcap tracing on all links
  p2p_top.EnablePcapAll("sender-r1");
  p2p_bottleneck.EnablePcapAll("r1-r2");
  p2p_bottom.EnablePcapAll("r2-receiver");

  // Congestion window trace
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  Ptr<OutputStreamWrapper> cwndStreamWrapper = Create<OutputStreamWrapper>(&cwndStream);
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, cwndStreamWrapper));

  // Install the app with our socket so tracing connects correctly
  sourceApp.Get(0)->SetAttribute("Socket", PointerValue(ns3TcpSocket));

  // Throughput trace
  Ptr<OutputStreamWrapper> throughputStreamWrapper = Create<OutputStreamWrapper>(&throughputStream);
  Simulator::Schedule(Seconds(1.1), &TraceThroughput, sourceApp.Get(0), throughputStreamWrapper);

  // Queue length trace on the bottleneck device's TX queue
  Ptr<PointToPointNetDevice> bottleneckDevice = DynamicCast<PointToPointNetDevice>(d1d2.Get(0)); // R1's device toward R2
  Ptr<Queue<Packet>> queue = bottleneckDevice->GetQueue();
  queue->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueSizeTrace));

  // Schedule PROBE_RTT phase every 10 seconds (pseudo-force by reducing cwnd to 4 MSS)
  uint32_t segmentSize = 1500; // Default Ethernet
  Simulator::Schedule(Seconds(10.0), &EnterProbeRtt, ns3TcpSocket, segmentSize);

  // Stop simulation after 100 seconds
  Simulator::Stop(Seconds(100.1));
  Simulator::Run();
  Simulator::Destroy();

  // Close output streams
  cwndStream.close();
  throughputStream.close();
  queueStream.close();

  return 0;
}