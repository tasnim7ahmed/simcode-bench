#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpBulkTransferExample");

class ThroughputMonitor
{
public:
  ThroughputMonitor()
    : m_lastTotalRx(0), m_startTime(Seconds(0)) {}

  void
  SetStream(Ptr<Application> app)
  {
    m_app = DynamicCast<BulkSendApplication>(app);
  }

  void
  SetSink(Ptr<Application> sink)
  {
    m_sink = DynamicCast<PacketSink>(sink);
  }

  void
  Start(Time start, Time stop, Time interval)
  {
    m_startTime = start;
    m_stopTime = stop;
    m_interval = interval;
    Simulator::Schedule(m_interval, &ThroughputMonitor::CheckThroughput, this);
  }

  void
  CheckThroughput()
  {
    if (m_sink)
      {
        uint64_t curRx = m_sink->GetTotalRx();
        double throughput = (curRx - m_lastTotalRx) * 8.0 / (m_interval.GetSeconds() * 1e6); // Mbps

        std::cout << "Time: " << Simulator::Now().GetSeconds ()
                  << "s  Total Received: " << curRx
                  << " bytes  Throughput: " << throughput << " Mbps" << std::endl;

        m_lastTotalRx = curRx;

        if (Simulator::Now() + m_interval <= m_stopTime + m_startTime)
          {
            Simulator::Schedule(m_interval, &ThroughputMonitor::CheckThroughput, this);
          }
      }
  }

private:
  Ptr<BulkSendApplication> m_app;
  Ptr<PacketSink> m_sink;
  uint64_t m_lastTotalRx;
  Time m_startTime, m_stopTime, m_interval;
};

int main(int argc, char *argv[])
{
  std::string dataRate = "10Mbps";
  std::string delay = "5ms";
  uint32_t maxBytes = 0;
  double simTime = 10.0;
  std::string pcapFile = "tcp-bulk-transfer.pcap";

  CommandLine cmd;
  cmd.AddValue("dataRate", "Data rate for point-to-point link", dataRate);
  cmd.AddValue("delay", "Propagation delay for point-to-point link", delay);
  cmd.AddValue("maxBytes", "Maximum bytes to send (0 means unlimited)", maxBytes);
  cmd.AddValue("simTime", "Duration of simulation [s]", simTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices = p2p.Install(nodes);

  p2p.EnablePcapAll("tcp-bulk-transfer", true);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simTime+1));

  BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
  bulkSender.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer senderApps = bulkSender.Install(nodes.Get(0));
  senderApps.Start(Seconds(0.1));
  senderApps.Stop(Seconds(simTime));

  ThroughputMonitor monitor;
  monitor.SetStream(senderApps.Get(0));
  monitor.SetSink(sinkApps.Get(0));
  monitor.Start(Seconds(0.1), Seconds(simTime), Seconds(1.0));

  Simulator::Stop(Seconds(simTime+1));
  Simulator::Run();

  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
  std::cout << "Total bytes received: " << sink->GetTotalRx() << std::endl;

  Simulator::Destroy();
  return 0;
}