#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueDiscExample");

class DumbbellQueueDiscExperiment
{
public:
  DumbbellQueueDiscExperiment();
  void Run(uint32_t nLeaf, std::string queueType, uint32_t queueLimit,
           uint32_t pktSize, DataRate dataRate, Time onTime, Time offTime,
           bool packetMode, uint32_t redMinTh, uint32_t redMaxTh, double redMaxP);
  void CheckQueueDiscStats(QueueDiscContainer qdiscs);

private:
  NodeContainer leftLeafNodes;
  NodeContainer rightLeafNodes;
  NodeContainer routers;
  NetDeviceContainer leftRouterDevices;
  NetDeviceContainer rightRouterDevices;
  Ipv4InterfaceContainer leftRouterInterfaces;
  Ipv4InterfaceContainer rightRouterInterfaces;
};

DumbbellQueueDiscExperiment::DumbbellQueueDiscExperiment()
{
  // Initialize containers
  routers.Create(2);
}

void
DumbbellQueueDiscExperiment::Run(uint32_t nLeaf, std::string queueType, uint32_t queueLimit,
                                 uint32_t pktSize, DataRate dataRate, Time onTime, Time offTime,
                                 bool packetMode, uint32_t redMinTh, uint32_t redMaxTh,
                                 double redMaxP)
{
  // Create leaf nodes
  leftLeafNodes.Create(nLeaf);
  rightLeafNodes.Create(nLeaf);

  // Point-to-point helper for connecting leaf nodes to routers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", DataValue(dataRate));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  // Connect left leaves to left router
  for (uint32_t i = 0; i < nLeaf; ++i)
  {
    NetDeviceContainer ndc = p2p.Install(leftLeafNodes.Get(i), routers.Get(0));
    Ipv4AddressHelper ipv4;
    std::ostringstream subnet;
    subnet << "10.1." << i << ".0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    Ipv4InterfaceContainer ifc = ipv4.Assign(ndc);
  }

  // Connect right leaves to right router
  for (uint32_t i = 0; i < nLeaf; ++i)
  {
    NetDeviceContainer ndc = p2p.Install(rightLeafNodes.Get(i), routers.Get(1));
    Ipv4AddressHelper ipv4;
    std::ostringstream subnet;
    subnet << "10.2." << i << ".0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    Ipv4InterfaceContainer ifc = ipv4.Assign(ndc);
  }

  // Connect two routers
  p2p.SetDeviceAttribute("DataRate", DataValue(DataRate("10Mbps")));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
  NetDeviceContainer routerDevices = p2p.Install(routers);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces = ipv4.Assign(routerDevices);

  // Install Internet stacks
  InternetStackHelper stack;
  stack.InstallAll();

  // Traffic Control Configuration
  TrafficControlHelper tch;
  QueueDiscHelper queueDiscHelper;

  if (queueType == "RED")
  {
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", UintegerValue(redMinTh),
                         "MaxTh", UintegerValue(redMaxTh),
                         "MaxP", DoubleValue(redMaxP),
                         "QueueLimit", UintegerValue(queueLimit));

    if (!packetMode)
    {
      Config::SetDefault("ns3::RedQueueDisc::BytesMode", BooleanValue(true));
    }
  }
  else if (queueType == "NLRED")
  {
    tch.SetRootQueueDisc("ns3::NlredQueueDisc",
                         "MinTh", UintegerValue(redMinTh),
                         "MaxTh", UintegerValue(redMaxTh),
                         "MaxP", DoubleValue(redMaxP),
                         "QueueLimit", UintegerValue(queueLimit));

    if (!packetMode)
    {
      Config::SetDefault("ns3::NlredQueueDisc::BytesMode", BooleanValue(true));
    }
  }
  else
  {
    NS_ABORT_MSG("Unknown queue type: " << queueType);
  }

  QueueDiscContainer qdiscs = tch.Install(rightRouterDevices);
  CheckQueueDiscStats(qdiscs);

  // Install OnOff applications
  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = sinkHelper.Install(rightLeafNodes);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
  clientHelper.SetAttribute("OnTime", RandomVariableStreamValue(ConstantRandomVariableValue(onTime)));
  clientHelper.SetAttribute("OffTime", RandomVariableStreamValue(ConstantRandomVariableValue(offTime)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(pktSize));
  clientHelper.SetAttribute("DataRate", DataValue(DataRate("1Mbps")));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nLeaf; ++i)
  {
    AddressValue remoteAddress(InetSocketAddress(rightRouterInterfaces.GetAddress(1), port));
    clientHelper.SetAttribute("Remote", remoteAddress);
    clientApps.Add(clientHelper.Install(leftLeafNodes.Get(i)));
  }

  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // Enable flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress
              << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: "
              << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() -
                                            i->second.timeFirstTxPacket.GetSeconds()) /
                     1000 / 1000
              << " Mbps\n";
  }

  Simulator::Destroy();
}

void
DumbbellQueueDiscExperiment::CheckQueueDiscStats(QueueDiscContainer qdiscs)
{
  for (QueueDiscContainer::Iterator i = qdiscs.Begin(); i != qdiscs.End(); ++i)
  {
    Ptr<QueueDisc> q = *i;
    std::cout << "Queue disc " << q << " has " << q->GetNQueueDiscClasses() << " classes\n";
    QueueDisc::Stats st = q->GetStats();
    std::cout << "Packets dropped: " << st.nTotalDroppedPackets << "\n";
    std::cout << "Bytes dropped: " << st.nTotalDroppedBytes << "\n";
  }
}

int
main(int argc, char* argv[])
{
  uint32_t nLeaf = 2;
  std::string queueType = "RED";
  uint32_t queueLimit = 1000;
  uint32_t pktSize = 1000;
  DataRate dataRate("10Mbps");
  Time onTime = Seconds(1);
  Time offTime = Seconds(1);
  bool packetMode = true;
  uint32_t redMinTh = 5;
  uint32_t redMaxTh = 15;
  double redMaxP = 0.02;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nLeaf", "Number of leaf nodes", nLeaf);
  cmd.AddValue("queueType", "Queue disc type - RED or NLRED", queueType);
  cmd.AddValue("queueLimit", "Queue limit in packets or bytes", queueLimit);
  cmd.AddValue("pktSize", "Packet size in bytes", pktSize);
  cmd.AddValue("dataRate", "Data rate of the links", dataRate);
  cmd.AddValue("onTime", "Maximum duration of On state", onTime);
  cmd.AddValue("offTime", "Maximum duration of Off state", offTime);
  cmd.AddValue("packetMode", "Use packet mode for RED/NLRED", packetMode);
  cmd.AddValue("redMinTh", "RED minimum threshold", redMinTh);
  cmd.AddValue("redMaxTh", "RED maximum threshold", redMaxTh);
  cmd.AddValue("redMaxP", "RED mark probability", redMaxP);
  cmd.Parse(argc, argv);

  DumbbellQueueDiscExperiment exp;
  exp.Run(nLeaf, queueType, queueLimit, pktSize, dataRate, onTime, offTime, packetMode, redMinTh,
          redMaxTh, redMaxP);
}