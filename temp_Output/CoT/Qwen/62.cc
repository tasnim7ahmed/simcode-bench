#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

class TcpMetricsTracer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("TcpMetricsTracer")
      .SetParent<Application>()
      .AddConstructor<TcpMetricsTracer>();
    return tid;
  }

  TcpMetricsTracer() {}
  virtual ~TcpMetricsTracer() {}

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
  }

  void DoDispose() override
  {
    Application::DoDispose();
  }

private:
  void ScheduleTrace(Ptr<Socket> socket, std::string cwndFile, std::string pacingRateFile);
  void TraceCwnd(Ptr<Socket> socket, std::string cwndFile);
  void TracePacingRate(Ptr<Socket> socket, std::string pacingRateFile);
};

void TcpMetricsTracer::ScheduleTrace(Ptr<Socket> socket, std::string cwndFile, std::string pacingRateFile)
{
  Simulator::Schedule(Seconds(0.1), &TcpMetricsTracer::TraceCwnd, this, socket, cwndFile);
  Simulator::Schedule(Seconds(0.1), &TcpMetricsTracer::TracePacingRate, this, socket, pacingRateFile);
}

void TcpMetricsTracer::TraceCwnd(Ptr<Socket> socket, std::string cwndFile)
{
  Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
  if (tcpSocket)
  {
    uint32_t cwnd = tcpSocket->GetCongestionWindow();
    std::ofstream outFile(cwndFile, std::ios_base::app);
    outFile << Simulator::Now().GetSeconds() << " " << cwnd << std::endl;
    outFile.close();
  }
  Simulator::Schedule(Seconds(0.1), &TcpMetricsTracer::TraceCwnd, this, socket, cwndFile);
}

void TcpMetricsTracer::TracePacingRate(Ptr<Socket> socket, std::string pacingRateFile)
{
  Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
  if (tcpSocket && tcpSocket->GetObject<TcpPacedSocketBase>())
  {
    DataRate rate = tcpSocket->GetObject<TcpPacedSocketBase>()->GetPacingRate();
    std::ofstream outFile(pacingRateFile, std::ios_base::app);
    outFile << Simulator::Now().GetSeconds() << " " << rate.GetBitRate() << std::endl;
    outFile.close();
  }
  Simulator::Schedule(Seconds(0.1), &TcpMetricsTracer::TracePacingRate, this, socket, pacingRateFile);
}

int main(int argc, char *argv[])
{
  bool enablePacing = true;
  double simTime = 10.0;
  uint32_t initialCWnd = 10;
  std::string transportProt = "TcpNewReno";
  std::string cwndFile = "cwnd.data";
  std::string pacingRateFile = "pacing_rate.data";
  std::string flowMonitorFile = "flow_monitor.data";

  CommandLine cmd(__FILE__);
  cmd.AddValue("enablePacing", "Enable TCP pacing", enablePacing);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("initialCWnd", "Initial congestion window size (packets)", initialCWnd);
  cmd.AddValue("transportProt", "Transport protocol to use: TcpNewReno, TcpBic, TcpWestwood, etc.", transportProt);
  cmd.AddValue("cwndFile", "Output file for congestion window data", cwndFile);
  cmd.AddValue("pacingRateFile", "Output file for pacing rate data", pacingRateFile);
  cmd.AddValue("flowMonitorFile", "Output file for flow statistics", flowMonitorFile);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4194304));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4194304));
  Config::SetDefault("ns3::TcpSocketBase::InitialCWnd", UintegerValue(initialCWnd));
  Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(0.2)));
  Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MilliSeconds(1)));
  Config::SetDefault("ns3::TcpL4Protocol::PaceSegments", BooleanValue(enablePacing));
  Config::SetDefault("ns3::TcpL4Protocol::RecoveryType", TypeIdValue(TcpClassicRecovery::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(0)));

  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

  NodeContainer nodes;
  nodes.Create(6);

  PointToPointHelper p2p_n0n1, p2p_n1n2, p2p_n2n3, p2p_n3n4, p2p_n4n5, p2p_n1n4;

  p2p_n0n1.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  p2p_n0n1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
  p2p_n0n1.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  p2p_n1n2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("50Mbps")));
  p2p_n1n2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(20)));
  p2p_n1n2.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  p2p_n2n3.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps"))); // Bottleneck link
  p2p_n2n3.SetChannelAttribute("Delay", TimeValue(MilliSeconds(100)));
  p2p_n2n3.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("50p"));

  p2p_n3n4.SetDeviceAttribute("DataRate", DataRateValue(DataRate("50Mbps")));
  p2p_n3n4.SetChannelAttribute("Delay", TimeValue(MilliSeconds(20)));
  p2p_n3n4.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  p2p_n4n5.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  p2p_n4n5.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
  p2p_n4n5.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  p2p_n1n4.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
  p2p_n1n4.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));
  p2p_n1n4.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  NetDeviceContainer d0d1 = p2p_n0n1.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d1d2 = p2p_n1n2.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d2d3 = p2p_n2n3.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer d3d4 = p2p_n3n4.Install(nodes.Get(3), nodes.Get(4));
  NetDeviceContainer d4d5 = p2p_n4n5.Install(nodes.Get(4), nodes.Get(5));
  NetDeviceContainer d1d4 = p2p_n1n4.Install(nodes.Get(1), nodes.Get(4));

  InternetStackHelper stack;
  stack.SetTcp(stack.GetTypeId(), "Pacing", BooleanValue(enablePacing));
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.NewNetwork();
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  address.NewNetwork();
  Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

  address.NewNetwork();
  Ipv4InterfaceContainer i3i4 = address.Assign(d3d4);

  address.NewNetwork();
  Ipv4InterfaceContainer i4i5 = address.Assign(d4d5);

  address.NewNetwork();
  Ipv4InterfaceContainer i1i4 = address.Assign(d1d4);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(i4i5.GetAddress(1), sinkPort));

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(4));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simTime));

  BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
  ApplicationContainer sourceApp = source.Install(nodes.Get(2));
  sourceApp.Start(Seconds(0.5));
  sourceApp.Stop(Seconds(simTime - 0.1));

  TcpMetricsTracer tracer;
  ApplicationContainer traceApp;
  traceApp.Add(CreateObject<TcpMetricsTracer>());
  traceApp.Start(Seconds(0.0));
  traceApp.Stop(Seconds(simTime));

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(
    [](Ptr<const Packet> p, const Address &) {
      std::ofstream out("packet_rx.data", std::ios_base::app);
      out << Simulator::Now().GetSeconds() << " " << p->GetSize() << std::endl;
    }));

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Tx", MakeCallback(
    [](Ptr<const Packet> p, const Address &) {
      std::ofstream out("packet_tx.data", std::ios_base::app);
      out << Simulator::Now().GetSeconds() << " " << p->GetSize() << std::endl;
    }));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  flowmon.SerializeToXmlFile(flowMonitorFile, false, false);

  Simulator::Destroy();

  std::cout << "Simulation completed. Results saved to:" << std::endl;
  std::cout << "- Congestion Window: " << cwndFile << std::endl;
  std::cout << "- Pacing Rate: " << pacingRateFile << std::endl;
  std::cout << "- Packet Transmission: packet_tx.data" << std::endl;
  std::cout << "- Packet Reception: packet_rx.data" << std::endl;
  std::cout << "- Flow Statistics: " << flowMonitorFile << std::endl;

  return 0;
}