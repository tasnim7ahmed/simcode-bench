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
    : m_stream(stream), m_interval(interval), m_lastTotalBytes(0), m_lastTime(Seconds(0)) {}

  void LogThroughput() {
    double now = Simulator::Now().GetSeconds();
    uint64_t totalBytes = m_socket->GetTxBytes();
    double diffTime = now - m_lastTime.GetSeconds();
    double diffBytes = totalBytes - m_lastTotalBytes;
    double throughput = (diffBytes * 8.0) / (diffTime * 1000000.0); // Mbps

    *m_stream->GetStream() << now << " " << throughput << std::endl;

    m_lastTotalBytes = totalBytes;
    m_lastTime = Seconds(now);

    Simulator::Schedule(m_interval, &ThroughputLogger::LogThroughput, this);
  }

  void SetSocket(Ptr<Socket> socket) {
    m_socket = socket;
  }

private:
  Ptr<OutputStreamWrapper> m_stream;
  Time m_interval;
  uint64_t m_lastTotalBytes;
  Time m_lastTime;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[]) {
  std::string rate = "1Mbps";
  std::string delay = "20ms";
  uint32_t packetSize = 1024;
  Time interval = Seconds(0.5);
  double simulationTime = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("rate", "Data rate of the point-to-point channel", rate);
  cmd.AddValue("delay", "Propagation delay of the channel", delay);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("interval", "Throughput logging interval in seconds", interval);
  cmd.AddValue("simulationTime", "Duration of the simulation in seconds", simulationTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(rate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

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
  sinkApps.Stop(Seconds(simulationTime));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite data
  ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
  sourceApps.Start(Seconds(0.5));
  sourceApps.Stop(Seconds(simulationTime - 0.1));

  AsciiTraceHelper asciiTraceHelper;
  pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-bulk-transfer.tr"));
  pointToPoint.EnablePcapAll("tcp-bulk-transfer");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

  Simulator::Stop(Seconds(simulationTime));

  Ptr<OutputStreamWrapper> throughputStream = CreateFileStreamWrapper("throughput-log.txt", std::ios::out);
  ThroughputLogger logger(throughputStream, interval);
  logger.SetSocket(DynamicCast<Socket>(sourceApps.Get(0)->GetObject<TcpSocketBase>()->GetBoundNetDevice()->GetNode()->GetObject<Socket>());
  Simulator::Schedule(Seconds(1.0), &ThroughputLogger::LogThroughput, &logger);

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1e6 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}