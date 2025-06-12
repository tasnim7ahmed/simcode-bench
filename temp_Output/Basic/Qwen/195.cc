#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferSimulation");

class ThroughputLogger {
public:
  ThroughputLogger(Time interval, std::string filename)
    : m_interval(interval), m_totalBytes(0), m_outputFile(filename) {}

  void TraceRx(Ptr<const Packet> packet, const TcpHeader& header, Ptr<const TcpSocketBase> socket) {
    m_totalBytes += packet->GetSize();
  }

  void LogThroughput() {
    double kbps = (m_totalBytes * 8.0) / (1000 * m_interval.GetSeconds());
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Throughput = " << kbps << " Kbps, Bytes Transmitted = " << m_totalBytes);
    m_totalBytes = 0;
    Simulator::Schedule(m_interval, &ThroughputLogger::LogThroughput, this);
  }

  void Start() {
    Simulator::Schedule(m_interval, &ThroughputLogger::LogThroughput, this);
  }

private:
  Time m_interval;
  uint64_t m_totalBytes;
  std::ofstream m_outputFile;
};

int main(int argc, char *argv[]) {
  std::string rate = "1Mbps";
  std::string delay = "20ms";
  uint32_t maxBytes = 10485760; // 10 MB

  CommandLine cmd(__FILE__);
  cmd.AddValue("rate", "P2P Data Rate", rate);
  cmd.AddValue("delay", "P2P Delay", delay);
  cmd.AddValue("maxBytes", "Maximum bytes to send", maxBytes);
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

  BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
  bulkSend.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer sourceApps = bulkSend.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  pointToPoint.EnablePcapAll("tcp-bulk-transfer");

  ThroughputLogger logger(Seconds(1.0), "throughput-log.txt");
  Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::PacketSink/RxWithAddresses",
                                MakeCallback(&ThroughputLogger::TraceRx, &logger));
  logger.Start();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}