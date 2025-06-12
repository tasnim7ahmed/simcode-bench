#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpTopology");

class DctcpTopologyExperiment
{
public:
  DctcpTopologyExperiment();
  void Run();

private:
  void SetupNetwork();
  void SetupFlows();
  void SetupMonitoring();
  void ReportResults();

  NodeContainer m_senders1;
  NodeContainer m_senders2;
  NodeContainer m_switchT1;
  NodeContainer m_switchT2;
  NodeContainer m_receiverR1;

  Ipv4InterfaceContainer m_intfSenders1ToT1;
  Ipv4InterfaceContainer m_intfT1ToT2;
  Ipv4InterfaceContainer m_intfT2ToR1;
  Ipv4InterfaceContainer m_intfReceivers;

  std::vector<Ptr<FlowMonitor>> m_flowMonitors;
};

DctcpTopologyExperiment::DctcpTopologyExperiment()
{
  m_switchT1.Create(1);
  m_switchT2.Create(1);
  m_receiverR1.Create(1);

  m_senders1.Create(30); // 30 senders through T1-T2 bottleneck
  m_senders2.Create(20); // 20 senders through T2-R1 bottleneck
}

void
DctcpTopologyExperiment::SetupNetwork()
{
  PointToPointHelper p2p;

  // T1 to T2: 10 Gbps link, RED queue with ECN
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));
  p2p.SetQueue("ns3::RedQueue",
               "MaxPackets", UintegerValue(1000),
               "UseEcn", BooleanValue(true));
  NetDeviceContainer t1t2Devices = p2p.Install(m_switchT1.Get(0), m_switchT2.Get(0));
  m_intfT1ToT2 = Ipv4InterfaceContainer();
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  m_intfT1ToT2 = ipv4.Assign(t1t2Devices);

  // T2 to R1: 1 Gbps link, RED queue with ECN
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
  p2p.SetQueue("ns3::RedQueue",
               "MaxPackets", UintegerValue(1000),
               "UseEcn", BooleanValue(true));
  NetDeviceContainer t2r1Devices = p2p.Install(m_switchT2.Get(0), m_receiverR1.Get(0));
  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  m_intfT2ToR1 = ipv4.Assign(t2r1Devices);

  // Sender1 -> T1 links
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
  p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(50)));
  for (uint32_t i = 0; i < m_senders1.GetN(); ++i)
    {
      NetDeviceContainer dev = p2p.Install(m_senders1.Get(i), m_switchT1.Get(0));
      Ipv4InterfaceContainer intf = ipv4.Assign(dev);
      m_intfSenders1ToT1.Add(intf.Get(0));
      ipv4.NewNetwork();
    }

  // Sender2 -> T2 links
  for (uint32_t i = 0; i < m_senders2.GetN(); ++i)
    {
      NetDeviceContainer dev = p2p.Install(m_senders2.Get(i), m_switchT2.Get(0));
      Ipv4InterfaceContainer intf = ipv4.Assign(dev);
      m_intfReceivers.Add(intf.Get(0));
      ipv4.NewNetwork();
    }

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void
DctcpTopologyExperiment::SetupFlows()
{
  uint32_t totalFlows = m_senders1.GetN() + m_senders2.GetN();
  double flowStartTime = 0.0;
  double flowStep = 1.0 / 40.0; // Staggered startup over 1 second

  // Configure receiver application on R1
  uint16_t port = 50000;
  for (uint32_t i = 0; i < totalFlows; ++i)
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory",
                            InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(m_receiverR1.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(5.0));
      port++;
    }

  // Configure sender applications
  port = 50000;
  for (uint32_t i = 0; i < m_senders1.GetN(); ++i)
    {
      OnOffHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(m_intfT2ToR1.GetAddress(1), port));
      source.SetAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
      source.SetAttribute("PacketSize", UintegerValue(1460));
      source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer app = source.Install(m_senders1.Get(i));
      app.Start(Seconds(flowStartTime));
      app.Stop(Seconds(5.0));

      flowStartTime += flowStep;
      port++;
    }

  for (uint32_t i = 0; i < m_senders2.GetN(); ++i)
    {
      OnOffHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(m_intfT2ToR1.GetAddress(1), port));
      source.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
      source.SetAttribute("PacketSize", UintegerValue(1460));
      source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer app = source.Install(m_senders2.Get(i));
      app.Start(Seconds(flowStartTime));
      app.Stop(Seconds(5.0));

      flowStartTime += flowStep;
      port++;
    }
}

void
DctcpTopologyExperiment::SetupMonitoring()
{
  FlowMonitorHelper flowmon;
  m_flowMonitors.clear();

  for (uint32_t i = 0; i < 40; ++i)
    {
      Ptr<FlowMonitor> fm = flowmon.Install(NodeContainer());
      m_flowMonitors.push_back(fm);
    }

  // Install monitors only after flows are set up
  Simulator::Schedule(Seconds(3.0), &FlowMonitor::CheckForLostPackets, flowmon.GetMonitor());
}

void
DctcpTopologyExperiment::ReportResults()
{
  Ptr<FlowMonitor> monitor = m_flowMonitors[0];

  double throughputSum = 0.0;
  double fairnessNumerator = 0.0;
  double fairnessDenominator = 0.0;
  uint32_t flowCount = 0;

  std::ofstream statsFile("dctcp_stats.csv");
  statsFile << "FlowId,ThroughputMbps,FairnessContribution" << std::endl;

  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto itr : stats)
    {
      if (itr.first >= 1 && itr.first <= 40) // Only consider the data flows
        {
          FlowMonitor::FlowStats s = itr.second;
          double duration = 1.0; // measurement window is [3,4]
          double throughput = (s.rxBytes * 8.0) / (duration * 1e6); // Mbps
          throughputSum += throughput;
          fairnessNumerator += sqrt(throughput);
          fairnessDenominator += throughput;
          flowCount++;

          statsFile << itr.first << "," << throughput << "," << sqrt(throughput) << std::endl;
        }
    }

  statsFile.close();

  double aggregateFairness = (fairnessNumerator * fairnessNumerator) / (flowCount * fairnessDenominator);
  NS_LOG_UNCOND("Aggregate Throughput: " << throughputSum << " Mbps");
  NS_LOG_UNCOND("Jain's Fairness Index: " << aggregateFairness);

  // Output queue statistics from the interfaces
  std::map<Ptr<const QueueDisc>, uint32_t> droppedPackets = monitor->GetDroppedPackets();
  for (auto& pair : droppedPackets)
    {
      NS_LOG_UNCOND("QueueDisc dropped packets: " << pair.second);
    }
}

void
DctcpTopologyExperiment::Run()
{
  SetupNetwork();
  SetupFlows();
  SetupMonitoring();

  Simulator::Stop(Seconds(5.0));
  Simulator::Run();
  ReportResults();
  Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
  LogComponentEnable("DctcpTopology", LOG_LEVEL_INFO);
  DctcpTopologyExperiment experiment;
  experiment.Run();
  return 0;
}