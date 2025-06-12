#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

class QueueDiscTracer {
public:
  QueueDiscTracer(std::string filename);
  void TraceQueueDisc(Ptr<QueueDisc> queue);

private:
  std::ofstream m_ofstream;
};

QueueDiscTracer::QueueDiscTracer(std::string filename) {
  m_ofstream.open(filename);
}

void QueueDiscTracer::TraceQueueDisc(Ptr<QueueDisc> queue) {
  uint32_t dropped = queue->GetTotalDroppedPackets();
  uint32_t marked = queue->GetTotalMarkedPackets();
  uint32_t qlen = queue->GetCurrentSize().GetValue();
  m_ofstream << Simulator::Now().GetSeconds() << " " << dropped << " " << marked << " " << qlen << std::endl;
}

int main(int argc, char *argv[]) {
  bool enablePcap = true;
  bool enableDctcpTraces = true;
  double stopTime = 10.0;

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
  tch.SetQueueLimits("ns3::DynamicQueueLimits");

  NetDeviceContainer devices[3];
  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[3];

  InternetStackHelper stack;
  stack.SetTcp("ns3::TcpNewReno");
  if (enableDctcpTraces) {
    Config::SetDefault("ns3::TcpSocketBase::DctcpEcn", BooleanValue(true));
  }
  stack.Install(nodes);

  devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
  devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
  devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));

  for (int i = 0; i < 3; ++i) {
    tch.Install(devices[i]);
  }

  address.SetBase("10.1.1.0", "255.255.255.0");
  interfaces[0] = address.Assign(devices[0]);

  address.SetBase("10.1.2.0", "255.255.255.0");
  interfaces[1] = address.Assign(devices[1]);

  address.SetBase("10.1.3.0", "255.255.255.0");
  interfaces[2] = address.Assign(devices[2]);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  PingHelper ping(nodes.Get(3)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
  ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  ping.SetAttribute("Size", UintegerValue(512));
  ApplicationContainer pingApp = ping.Install(nodes.Get(0));
  pingApp.Start(Seconds(0.0));
  pingApp.Stop(Seconds(stopTime));

  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces[2].GetAddress(1), 9));
  source.SetAttribute("MaxBytes", UintegerValue(10000000));
  ApplicationContainer sourceApp = source.Install(nodes.Get(0));
  sourceApp.Start(Seconds(0.5));
  sourceApp.Stop(Seconds(stopTime - 1.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(3));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(stopTime));

  QueueDiscContainer qdiscs = tch.GetRootQueueDiscs(devices[1]);
  QueueDiscTracer tracer("queue-disc-trace.txt");
  Simulator::Schedule(Seconds(0.0), &QueueDiscTracer::TraceQueueDisc, &tracer, qdiscs.Get(0));
  Simulator::Schedule(Seconds(0.1), &QueueDiscTracer::TraceQueueDisc, &tracer, qdiscs.Get(0));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Tx", MakeCallback(
    [](Ptr<const Packet> p) {
      NS_LOG_INFO("Sent packet of size " << p->GetSize());
    }));

  if (enablePcap) {
    p2p.EnablePcapAll("network-simulation");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> rttStream = ascii.CreateFileStream("rtt-trace.txt");
  Config::Connect("/NodeList/3/ApplicationList/0/$ns3::PacketSink/Rx", MakeBoundCallback(
    [](Ptr<OutputStreamWrapper> stream, const Address &) {
      *stream->GetStream() << Simulator::Now().GetSeconds() << std::endl;
    }, rttStream));

  Ptr<OutputStreamWrapper> cwndStream = ascii.CreateFileStream("cwnd-trace.txt");
  Config::Connect("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(
    [](Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
      *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
    }, cwndStream));

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  monitor->SerializeToXmlFile("flowmon-output.xml", false, false);

  Simulator::Destroy();
  return 0;
}