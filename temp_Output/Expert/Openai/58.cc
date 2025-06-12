#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBbrBottleneckExample");

void
CwndTracer(std::string fileName, uint32_t nodeId, uint32_t socketId, uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::map<std::string, std::ofstream> fileStreams;
  std::ostringstream oss;
  oss << fileName << "-node" << nodeId << "-socket" << socketId << ".dat";
  std::string outFile = oss.str();
  auto &out = fileStreams[outFile];
  if (!out.is_open())
    {
      out.open(outFile, std::ios_base::out);
      out << "# Time(s)    Cwnd(bytes)\n";
    }
  out << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

void
TxTracer(std::string fileName, Ptr<Application> app)
{
  static std::ofstream out(fileName);
  if (!out.is_open())
    {
      out.open(fileName, std::ios_base::out);
      out << "# Time(s)    Throughput(bps)\n";
    }
  Ptr<BulkSendApplication> bulkApp = DynamicCast<BulkSendApplication>(app);
  if (bulkApp)
    {
      double throughput = (bulkApp->GetTotalTx() * 8.0) / Simulator::Now().GetSeconds();
      out << Simulator::Now().GetSeconds() << " " << throughput << std::endl;
    }
  Simulator::Schedule(Seconds(0.1), &TxTracer, fileName, app);
}

void
QueueTracer(std::string fileName, Ptr<QueueDisc> queue)
{
  static std::ofstream out(fileName);
  if (!out.is_open())
    {
      out.open(fileName, std::ios_base::out);
      out << "# Time(s)  QueueSize(packets)\n";
    }
  out << Simulator::Now().GetSeconds() << " " << queue->GetCurrentSize().GetValue() << std::endl;
  Simulator::Schedule(Seconds(0.01), &QueueTracer, fileName, queue);
}

void
ScheduleProbeRtt(Ptr<Socket> socket, uint32_t segSize)
{
  Ptr<TcpBbr> bbr = DynamicCast<TcpBbr>(socket->GetObject<TcpSocketBase>());
  if (bbr)
    {
      bbr->EnterProbeRtt();
      bbr->SetProbeRttCwnd(segSize * 4);
    }
  Simulator::Schedule(Seconds(10.0), &ScheduleProbeRtt, socket, segSize);
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("TcpBbrBottleneckExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(4); // 0: sender, 1: R1, 2: R2, 3: receiver

  // Links
  PointToPointHelper p2p_high;
  p2p_high.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
  p2p_high.SetChannelAttribute("Delay", StringValue("5ms"));

  PointToPointHelper p2p_bottle;
  p2p_bottle.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p_bottle.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer d0d1 = p2p_high.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d1d2 = p2p_bottle.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d2d3 = p2p_high.Install(nodes.Get(2), nodes.Get(3));

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IPs
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

  // Enable pcap tracing
  p2p_high.EnablePcapAll("tcp-bbr-p2p-high", false);
  p2p_bottle.EnablePcapAll("tcp-bbr-bottleneck", false);

  // Traffic Control on bottleneck
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
  Ptr<QueueDisc> queueDisc = tch.Install(d1d2.Get(0)).Get(0);

  // Necessary for topology with routers
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configure TCP BBR on sender
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

  // Sync to make sure BBR available
  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(100.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

  // Set TCP BBR PROBE_RTT period (every 10 seconds)
  ns3TcpSocket->SetAttribute("SegmentSize", UintegerValue(1448));

  Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication>();
  app->SetStartTime(Seconds(0.1));
  app->SetStopTime(Seconds(100.0));
  app->SetAttribute("Remote", AddressValue(sinkAddress));
  app->SetAttribute("MaxBytes", UintegerValue(0));
  app->SetSocket(ns3TcpSocket);
  nodes.Get(0)->AddApplication(app);

  // Connect to Cwnd trace
  ns3TcpSocket->TraceConnect("CongestionWindow", MakeBoundCallback(&CwndTracer, "cwnd-trace",
        nodes.Get(0)->GetId(), ns3TcpSocket->GetId()));

  // Sender throughput trace
  Simulator::Schedule(Seconds(0.1), &TxTracer, "throughput-trace.dat", app);

  // Bottleneck queue size trace
  Simulator::ScheduleNow(&QueueTracer, "queue-size-trace.dat", queueDisc);

  // Schedule BBR PROBE_RTT every 10s
  Simulator::Schedule(Seconds(10.0), &ScheduleProbeRtt, ns3TcpSocket, 1448);

  Simulator::Stop(Seconds(100.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}