#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

class ThroughputMonitor
{
public:
  ThroughputMonitor(Ptr<Application> app, Time interval)
      : m_app(app), m_interval(interval), m_lastTotalRx(0)
  {
    m_server = DynamicCast<PacketSink>(app);
  }

  void Start()
  {
    m_startTime = Simulator::Now();
    ScheduleNext();
  }

  void Report()
  {
    uint64_t totalRx = m_server->GetTotalRx();
    double t = (Simulator::Now() - m_startTime).GetSeconds();
    double throughput = ((totalRx - m_lastTotalRx) * 8.0) / (m_interval.GetSeconds() * 1000000.0); // Mbps

    std::cout << Simulator::Now().GetSeconds() << " s: "
              << "Total Received: " << totalRx << " bytes, "
              << "Throughput: " << throughput << " Mbps" << std::endl;

    m_lastTotalRx = totalRx;
    ScheduleNext();
  }

private:
  void ScheduleNext()
  {
    if (Simulator::Now() < Simulator::GetMaximumSimulationTime())
    {
      Simulator::Schedule(m_interval, &ThroughputMonitor::Report, this);
    }
  }

  Ptr<Application> m_app;
  Ptr<PacketSink> m_server;
  Time m_interval;
  Time m_startTime;
  uint64_t m_lastTotalRx;
};

int main(int argc, char *argv[])
{
  std::string dataRate = "10Mbps";
  std::string delay = "5ms";
  uint32_t maxBytes = 0;
  double simDuration = 10.0;

  CommandLine cmd;
  cmd.AddValue("dataRate", "Data rate for point-to-point link", dataRate);
  cmd.AddValue("delay", "Link delay", delay);
  cmd.AddValue("maxBytes", "Maximum bytes the bulk send application will send (0 for unlimited)", maxBytes);
  cmd.AddValue("simDuration", "Simulation duration (seconds)", simDuration);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 5000;

  // Install the server (PacketSink)
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simDuration + 1));

  // Install BulkSendApplication on client
  BulkSendHelper source("ns3::TcpSocketFactory",
                        InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
  ApplicationContainer sourceApp = source.Install(nodes.Get(0));
  sourceApp.Start(Seconds(0.5));
  sourceApp.Stop(Seconds(simDuration + 1));

  // PCAP tracing
  p2p.EnablePcapAll("tcp-bulk-send");

  // Throughput/bytes logging
  Ptr<Application> serverApp = sinkApp.Get(0);
  ThroughputMonitor monitor(serverApp, Seconds(1.0));
  Simulator::Schedule(Seconds(1.0), &ThroughputMonitor::Start, &monitor);

  Simulator::Stop(Seconds(simDuration + 1));
  Simulator::Run();
  Simulator::Destroy();

  // Final bytes received
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp);
  std::cout << "Total Bytes Received: " << sink->GetTotalRx() << std::endl;

  return 0;
}