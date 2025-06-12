#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueDiscSimulator");

class DumbbellHelper {
public:
  DumbbellHelper(uint32_t nLeaf, std::string queueType, uint32_t limitPackets, uint32_t limitBytes,
                 uint32_t minTh, uint32_t maxTh, bool useByteQueue, std::string redLabel);

  NodeContainer Install(const NodeContainer& leftLeafs, const NodeContainer& rightLeafs,
                        Ptr<Node> bottleneckLeft, Ptr<Node> bottleneckRight);
private:
  uint32_t m_nLeaf;
  std::string m_queueType;
  uint32_t m_limitPackets;
  uint32_t m_limitBytes;
  uint32_t m_minTh;
  uint32_t m_maxTh;
  bool m_useByteQueue;
  std::string m_redLabel;
};

DumbbellHelper::DumbbellHelper(uint32_t nLeaf, std::string queueType, uint32_t limitPackets,
                               uint32_t limitBytes, uint32_t minTh, uint32_t maxTh,
                               bool useByteQueue, std::string redLabel)
  : m_nLeaf(nLeaf),
    m_queueType(queueType),
    m_limitPackets(limitPackets),
    m_limitBytes(limitBytes),
    m_minTh(minTh),
    m_maxTh(maxTh),
    m_useByteQueue(useByteQueue),
    m_redLabel(redLabel)
{
}

NodeContainer
DumbbellHelper::Install(const NodeContainer& leftLeafs, const NodeContainer& rightLeafs,
                        Ptr<Node> bottleneckLeft, Ptr<Node> bottleneckRight)
{
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devBottleneck;
  devBottleneck = p2p.Install(bottleneckLeft, bottleneckRight);

  TrafficControlHelper tch;
  if (m_queueType == "RED")
    {
      tch.SetRootQueueDisc("ns3::RedQueueDisc",
                           "MinTh", UintegerValue(m_minTh),
                           "MaxTh", UintegerValue(m_maxTh),
                           "QueueLimit", UintegerValue(m_limitPackets),
                           "UseByte", BooleanValue(m_useByteQueue));
    }
  else if (m_queueType == "NLRED")
    {
      tch.SetRootQueueDisc("ns3::NlredQueueDisc",
                           "MinTh", UintegerValue(m_minTh),
                           "MaxTh", UintegerValue(m_maxTh),
                           "QueueLimit", UintegerValue(m_limitPackets),
                           "UseByte", BooleanValue(m_useByteQueue));
    }
  else
    {
      NS_FATAL_ERROR("Unknown queue type: " << m_queueType);
    }

  QueueDiscContainer qdiscs = tch.Install(devBottleneck);

  InternetStackHelper internet;
  internet.Install(leftLeafs);
  internet.Install(rightLeafs);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");

  for (uint32_t i = 0; i < m_nLeaf; ++i)
    {
      NetDeviceContainer devLeft = p2p.Install(leftLeafs.Get(i), bottleneckLeft);
      NetDeviceContainer devRight = p2p.Install(rightLeafs.Get(i), bottleneckRight);
      ipv4.Assign(devLeft);
      ipv4.Assign(devRight);
      ipv4.NewNetwork();
    }

  Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(devBottleneck);

  return ipInterfaces.GetAddress(1);
}

int
main(int argc, char* argv[])
{
  uint32_t nLeaf = 2;
  std::string queueType = "RED";
  uint32_t limitPackets = 1000;
  uint32_t limitBytes = 1024 * 1024 * 10;
  uint32_t minTh = 50;
  uint32_t maxTh = 100;
  bool useByteQueue = false;
  std::string redLabel = "defaultRed";
  uint32_t packetSize = 1024;
  DataRate dataRate("1Mbps");
  TimeValue onTime = MilliSeconds(100);
  TimeValue offTime = MilliSeconds(100);

  CommandLine cmd(__FILE__);
  cmd.AddValue("nLeaf", "Number of leaf nodes on each side.", nLeaf);
  cmd.AddValue("queueType", "Queue disc type: RED or NLRED", queueType);
  cmd.AddValue("limitPackets", "Queue disc capacity in packets", limitPackets);
  cmd.AddValue("limitBytes", "Queue disc capacity in bytes", limitBytes);
  cmd.AddValue("minTh", "RED minimum threshold", minTh);
  cmd.AddValue("maxTh", "RED maximum threshold", maxTh);
  cmd.AddValue("useByteQueue", "Use byte-based queue limits", useByteQueue);
  cmd.AddValue("packetSize", "Packet size for OnOff application", packetSize);
  cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
  cmd.AddValue("onTime", "On time for OnOff application", onTime);
  cmd.AddValue("offTime", "Off time for OnOff application", offTime);
  cmd.Parse(argc, argv);

  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(1);

  NodeContainer leftLeafs;
  leftLeafs.Create(nLeaf);

  NodeContainer rightLeafs;
  rightLeafs.Create(nLeaf);

  Ptr<Node> bottleneckLeft = CreateObject<Node>();
  Ptr<Node> bottleneckRight = CreateObject<Node>();

  NodeContainer bottlenecks(bottleneckLeft, bottleneckRight);

  DumbbellHelper dumbbell(nLeaf, queueType, limitPackets, limitBytes, minTh, maxTh, useByteQueue, redLabel);
  Ipv4Address sinkAddress = dumbbell.Install(leftLeafs, rightLeafs, bottleneckLeft, bottleneckRight);

  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), 80));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(rightLeafs);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(sinkAddress, 80));
      clientHelper.SetAttribute("OnTime", onTime);
      clientHelper.SetAttribute("OffTime", offTime);
      clientHelper.SetAttribute("DataRate", DataRateValue(dataRate));
      clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

      ApplicationContainer clientApp = clientHelper.Install(leftLeafs.Get(i));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(9.0));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.destinationPort == 80)
        {
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):" << std::endl;
          std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
          std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
          std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
          std::cout << "  Pkt Drop Ratio: " << ((double)i->second.lostPackets / (i->second.txPackets + i->second.lostPackets)) * 100 << "%" << std::endl;
        }
    }

  Simulator::Destroy();
  return 0;
}