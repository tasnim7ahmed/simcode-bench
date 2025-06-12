#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

class QueueDiscSimulator
{
public:
  QueueDiscSimulator(std::string queueType, bool useBql, DataRate bottleneckRate, Time bottleneckDelay)
      : m_queueType(queueType), m_useBql(useBql), m_bottleneckRate(bottleneckRate), m_bottleneckDelay(bottleneckDelay)
  {
  }

  void Run()
  {
    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    NodeContainer n1n2 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n2n3 = NodeContainer(nodes.Get(1), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create point-to-point channels
    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("0.1ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", DataRateValue(m_bottleneckRate));
    p2pBottleneck.SetChannelAttribute("Delay", TimeValue(m_bottleneckDelay));

    // Set up queue disc on bottleneck link
    TrafficControlHelper tchBottleneck;
    if (m_queueType == "PfifoFast")
    {
      tchBottleneck.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    }
    else if (m_queueType == "ARED")
    {
      tchBottleneck.SetRootQueueDisc("ns3::AredQueueDisc");
    }
    else if (m_queueType == "CoDel")
    {
      tchBottleneck.SetRootQueueDisc("ns3::CoDelQueueDisc");
    }
    else if (m_queueType == "FqCoDel")
    {
      tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }
    else if (m_queueType == "PIE")
    {
      tchBottleneck.SetRootQueueDisc("ns3::PieQueueDisc");
    }
    else
    {
      NS_FATAL_ERROR("Unknown queue type: " << m_queueType);
    }

    if (m_useBql)
    {
      tchBottleneck.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));
    }

    TrafficControlHelper tchAccess;
    tchAccess.SetRootQueueDisc("ns3::PfifoFastQueueDisc");

    NetDeviceContainer devAccess1, devAccess2, devBottleneck;

    // Install devices and queues
    devAccess1 = p2pAccess.Install(n1n2);
    devBottleneck = p2pBottleneck.Install(n2n3);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(devAccess1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(devBottleneck);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup applications
    uint16_t port = 5000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i2i3.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSource = source.Install(nodes.Get(0));
    appSource.Start(Seconds(1.0));
    appSource.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appSink = sink.Install(nodes.Get(2));
    appSink.Start(Seconds(1.0));
    appSink.Stop(Seconds(10.0));

    // Bidirectional flow
    BulkSendHelper sourceRev("ns3::TcpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), port + 1));
    sourceRev.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSourceRev = sourceRev.Install(nodes.Get(2));
    appSourceRev.Start(Seconds(1.0));
    appSourceRev.Stop(Seconds(10.0));

    PacketSinkHelper sinkRev("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer appSinkRev = sinkRev.Install(nodes.Get(0));
    appSinkRev.Start(Seconds(1.0));
    appSinkRev.Stop(Seconds(10.0));

    // Ping application
    V4PingHelper ping(i1i2.GetAddress(1));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(2.0));
    pingApp.Stop(Seconds(10.0));

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Output trace files
    AsciiTraceHelper ascii;
    std::ostringstream oss;

    oss << "queue-disc-" << m_queueType << "-bql-" << (m_useBql ? "on" : "off") << ".tr";
    p2pBottleneck.EnablePcapAll(oss.str().c_str());

    oss.str("");
    oss << "queue-disc-" << m_queueType << "-bql-" << (m_useBql ? "on" : "off") << "-queue.tr";
    tchBottleneck.EnableQueueTraces(nodes.Get(1)->GetDevice(1), oss.str().c_str());

    oss.str("");
    oss << "queue-disc-" << m_queueType << "-bql-" << (m_useBql ? "on" : "off") << "-bytes-in-queue.tr";
    tchBottleneck.EnableBytesInQueueTraces(nodes.Get(1)->GetDevice(1), oss.str().c_str());

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double goodput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
      if (t.destinationPort == port || t.destinationPort == port + 1)
      {
        goodput += iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1e6;
      }
    }

    NS_LOG_UNCOND("Goodput with queue disc " << m_queueType << " and BQL " << (m_useBql ? "on" : "off") << ": " << goodput << " Mbps");
  }

private:
  std::string m_queueType;
  bool m_useBql;
  DataRate m_bottleneckRate;
  Time m_bottleneckDelay;
};

int main(int argc, char *argv[])
{
  std::string queueType = "PfifoFast";
  bool useBql = false;
  DataRate bottleneckRate("1Mbps");
  Time bottleneckDelay("10ms");

  CommandLine cmd(__FILE__);
  cmd.AddValue("queueType", "Queue disc type: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueType);
  cmd.AddValue("useBql", "Enable Byte Queue Limits (BQL)", useBql);
  cmd.AddValue("bottleneckRate", "Bottleneck link data rate", bottleneckRate);
  cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
  cmd.Parse(argc, argv);

  QueueDiscSimulator simulator(queueType, useBql, bottleneckRate, bottleneckDelay);
  simulator.Run();

  return 0;
}