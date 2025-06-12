#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferExample");

class BulkTransferTraceHelper
{
public:
  BulkTransferTraceHelper()
      : m_lastTotalRx(0), m_startTime(Seconds(0.0)), m_logInterval(Seconds(1.0)) {}

  void SetStartTime(Time t) { m_startTime = t; }
  void SetLogInterval(Time t) { m_logInterval = t; }
  void Install(Ptr<Application> sinkApp)
  {
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp);
    if (sink)
    {
      m_sink = sink;
      Simulator::Schedule(m_logInterval, &BulkTransferTraceHelper::LogThroughput, this);
    }
  }

private:
  void LogThroughput()
  {
    Time now = Simulator::Now();
    if (!m_sink)
      return;

    uint64_t totalRx = m_sink->GetTotalRx();
    double throughput = ((totalRx - m_lastTotalRx) * 8.0) / m_logInterval.GetSeconds() / 1e6; // Mbps

    std::cout << "Time: " << now.GetSeconds()
              << "s\tTotal Rx: " << totalRx
              << " bytes\tThroughput: " << throughput
              << " Mbps" << std::endl;

    m_lastTotalRx = totalRx;
    Simulator::Schedule(m_logInterval, &BulkTransferTraceHelper::LogThroughput, this);
  }

  Ptr<PacketSink> m_sink;
  uint64_t m_lastTotalRx;
  Time m_startTime;
  Time m_logInterval;
};

int main(int argc, char *argv[])
{
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  uint32_t maxBytes = 0; // 0 means unlimited
  uint16_t port = 5001;

  CommandLine cmd;
  cmd.AddValue("dataRate", "Data rate for point-to-point link", dataRate);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("maxBytes", "Maximum bytes sent by BulkSendApplication", maxBytes);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install PacketSink on node 1 (receiver)
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(20.0));

  // Install BulkSendApplication on node 0 (sender)
  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(1), port));
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
  sourceApp.Start(Seconds(1.0));
  sourceApp.Stop(Seconds(20.0));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("tcp-bulk-transfer");

  // Throughput and statistics logger setup
  BulkTransferTraceHelper traceHelper;
  traceHelper.Install(sinkApp.Get(0));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
  if (sink)
  {
    std::cout << "Total Bytes Received: " << sink->GetTotalRx() << std::endl;
  }

  return 0;
}