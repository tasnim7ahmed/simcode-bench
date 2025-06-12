#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRoutersWithLAN");

class ThroughputMonitor : public Application
{
public:
  static TypeId GetTypeId();
  ThroughputMonitor();
  virtual ~ThroughputMonitor();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void CalculateThroughput();

  Ptr<Socket> m_socket;
  uint64_t m_bytesReceived;
  EventId m_event;
};

TypeId
ThroughputMonitor::GetTypeId()
{
  static TypeId tid = TypeId("ThroughputMonitor")
                          .SetParent<Application>()
                          .AddConstructor<ThroughputMonitor>();
  return tid;
}

ThroughputMonitor::ThroughputMonitor()
    : m_bytesReceived(0)
{
}

ThroughputMonitor::~ThroughputMonitor()
{
  if (m_socket)
  {
    m_socket->Close();
  }
}

void
ThroughputMonitor::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
  m_socket->Bind(local);
  m_socket->Listen();
  m_socket->SetRecvCallback(MakeCallback(&ThroughputMonitor::HandleRead, this));
  m_event = Simulator::Schedule(Seconds(1.0), &ThroughputMonitor::CalculateThroughput, this);
}

void
ThroughputMonitor::StopApplication()
{
  if (m_event.IsRunning())
  {
    Simulator::Cancel(m_event);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void
ThroughputMonitor::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    m_bytesReceived += packet->GetSize();
  }
}

void
ThroughputMonitor::CalculateThroughput()
{
  static double lastTime = 0;
  double currentTime = Simulator::Now().GetSeconds();
  double interval = currentTime - lastTime;
  double throughput = (m_bytesReceived * 8.0) / (interval * 1000000.0); // Mbps

  std::ofstream outFile("throughput.csv", std::ios_base::app);
  if (outFile.is_open())
  {
    outFile << currentTime << "," << throughput << std::endl;
    outFile.close();
  }

  lastTime = currentTime;
  m_bytesReceived = 0;
  m_event = Simulator::Schedule(Seconds(1.0), &ThroughputMonitor::CalculateThroughput, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

  NodeContainer nodesRouters;
  nodesRouters.Create(3); // R1, R2, R3

  NodeContainer csmaNodes;
  csmaNodes.Add(nodesRouters.Get(1)); // R2 as LAN gateway
  csmaNodes.Create(2);               // Two LAN hosts: H1 and H2

  NodeContainer p2pR1R2;
  p2pR1R2.Add(nodesRouters.Get(0)); // R1
  p2pR1R2.Add(nodesRouters.Get(1)); // R2

  NodeContainer p2pR2R3;
  p2pR2R3.Add(nodesRouters.Get(1)); // R2
  p2pR2R3.Add(nodesRouters.Get(2)); // R3

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer p2pDevicesR1R2 = pointToPoint.Install(p2pR1R2);
  NetDeviceContainer p2pDevicesR2R3 = pointToPoint.Install(p2pR2R3);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(200)));

  NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfacesR1R2 = address.Assign(p2pDevicesR1R2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfacesR2R3 = address.Assign(p2pDevicesR2R3);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Server node (H2 connected to R2's LAN) receiving traffic
  uint16_t port = 50000;
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(csmaNodes.Get(2)); // H2
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Client node (H1 in LAN) sending data to H2 via R2 and R3
  OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(csmaInterfaces.GetAddress(2), port));
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = clientHelper.Install(csmaNodes.Get(1)); // H1
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Throughput monitor application on H2
  ThroughputMonitor throughputMonitor;
  csmaNodes.Get(2)->AddApplication(&throughputMonitor);
  throughputMonitor.SetStartTime(Seconds(1.0));
  throughputMonitor.SetStopTime(Seconds(10.0));

  // Flow Monitor setup
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  // Animation interface
  AnimationInterface anim("three_routers_with_lan.xml");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Output flow statistics
  flowmon->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

  std::ofstream statsFile("flow_stats.csv");
  statsFile << "FlowID,SrcIP,DstIP,PacketsSent,ReceivedPackets,LostPackets,"
            << "ThroughputMbps,PacketLossRatio,DelayMean\n";

  for (auto &[flowId, flowStats] : stats)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
    statsFile << flowId << ","
              << t.sourceAddress << "," << t.destinationAddress << ","
              << flowStats.txPackets << "," << flowStats.rxPackets << ","
              << flowStats.lostPackets << ","
              << (flowStats.rxBytes * 8.0) / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1e6 << ","
              << (double)flowStats.packetLossRatio << ","
              << flowStats.delaySum.GetSeconds() / flowStats.rxPackets << "\n";
  }

  statsFile.close();

  Simulator::Destroy();
  return 0;
}