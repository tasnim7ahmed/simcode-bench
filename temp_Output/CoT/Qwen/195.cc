#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferSimulation");

class ThroughputLogger {
public:
  ThroughputLogger(Ptr<OutputStreamWrapper> stream, Time interval)
    : m_bytesReceived(0), m_interval(interval), m_stream(stream) {}

  void TraceRx(Ptr<const Packet> packet, const Address &from) {
    m_bytesReceived += packet->GetSize();
  }

  void LogThroughput() {
    double kbps = (m_bytesReceived * 8.0) / (1000.0);
    *m_stream->GetStream() << Simulator::Now().GetSeconds() << "s\t" << kbps << " Kbps" << std::endl;
    m_bytesReceived = 0;
    Simulator::Schedule(m_interval, &ThroughputLogger::LogThroughput, this);
  }

private:
  uint64_t m_bytesReceived;
  Time m_interval;
  Ptr<OutputStreamWrapper> m_stream;
};

int main(int argc, char *argv[]) {
  std::string rate = "1Mbps";
  std::string delay = "20ms";
  uint32_t packetSize = 1024;
  bool tracing = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("rate", "Data rate for the point-to-point link", rate);
  cmd.AddValue("delay", "Propagation delay for the point-to-point link", delay);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("TcpBulkTransferSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(rate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", Address());
  bulkSendHelper.SetAttribute("Remote", AddressValue(sinkAddress));
  bulkSendHelper.SetAttribute("SendSize", UintegerValue(packetSize));
  ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(9.0));

  AsciiTraceHelper asciiTraceHelper;
  pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-bulk-transfer.tr"));
  if (tracing) {
    pointToPoint.EnablePcapAll("tcp-bulk-transfer");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::PacketSink/RxWithAddresses",
                                MakeBoundCallback(&ThroughputLogger::TraceRx, Create<ThroughputLogger>(
                                    asciiTraceHelper.CreateFileStream("throughput.log"), Seconds(0.5))));
  Simulator::Schedule(Seconds(0.5), &ThroughputLogger::LogThroughput,
                      Create<ThroughputLogger>(asciiTraceHelper.CreateFileStream("throughput.log"), Seconds(0.5)));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
    NS_LOG_INFO("Flow ID: " << iter->first << " Info: " << iter->second);
  }

  Simulator::Destroy();
  return 0;
}