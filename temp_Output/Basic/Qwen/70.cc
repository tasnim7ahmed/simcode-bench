#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueDiscExample");

class DumbbellQueueDiscExperiment
{
public:
  DumbbellQueueDiscExperiment();
  void Run(uint32_t nLeaf, std::string redMinTh, std::string redMaxTh, uint32_t pktSize,
           std::string dataRate, std::string queueType, bool modeBytes, uint32_t queueLimit);
  void Report();

private:
  NodeContainer leafNodes;
  NodeContainer routerNodes;
  NetDeviceContainer leftRouterDevices;
  NetDeviceContainer rightRouterDevices;
  NetDeviceContainer routerLink;
  Ipv4InterfaceContainer leftInterfaces;
  Ipv4InterfaceContainer rightInterfaces;
  Ipv4InterfaceContainer routerInterface;
  uint64_t totalDroppedPackets;
};

DumbbellQueueDiscExperiment::DumbbellQueueDiscExperiment()
  : totalDroppedPackets(0)
{
}

void
DumbbellQueueDiscExperiment::Run(uint32_t nLeaf, std::string redMinTh, std::string redMaxTh,
                                 uint32_t pktSize, std::string dataRate, std::string queueType,
                                 bool modeBytes, uint32_t queueLimit)
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("DumbbellQueueDiscExample", LOG_LEVEL_INFO);

  // Create nodes
  leafNodes.Create(nLeaf);
  routerNodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  // Left side (leaf to left router)
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      NetDeviceContainer ndc = p2p.Install(leafNodes.Get(i), routerNodes.Get(0));
      leftRouterDevices.Add(ndc.Get(1));
    }

  // Right side (leaf to right router) - OnOff applications go here
  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      NetDeviceContainer ndc = p2p.Install(leafNodes.Get(i), routerNodes.Get(1));
      rightRouterDevices.Add(ndc.Get(1));
    }

  // Link between routers
  routerLink = p2p.Install(routerNodes.Get(0), routerNodes.Get(1));

  InternetStackHelper internet;
  internet.InstallAll();

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  leftInterfaces = ipv4.Assign(leftRouterDevices);

  ipv4.NewNetwork();
  rightInterfaces = ipv4.Assign(rightRouterDevices);

  ipv4.NewNetwork();
  routerInterface = ipv4.Assign(routerLink);

  // Set up queue discs
  TrafficControlHelper tch;
  if (queueType == "RED")
    {
      tch.SetRootQueueDisc("ns3::RedQueueDisc",
                           "MinTh", StringValue(redMinTh),
                           "MaxTh", StringValue(redMaxTh),
                           "QueueLimit", UintegerValue(queueLimit),
                           "MeanPktSize", UintegerValue(pktSize),
                           "Wait", BooleanValue(true),
                           "Gentle", BooleanValue(true),
                           "UseHardDrop", BooleanValue(false),
                           "Mode", EnumValue(modeBytes ? QueueDisc::QUEUE_DISC_MODE_BYTES
                                                       : QueueDisc::QUEUE_DISC_MODE_PACKETS));
    }
  else if (queueType == "NLRED")
    {
      tch.SetRootQueueDisc("ns3::NlredQueueDisc",
                           "MinTh", StringValue(redMinTh),
                           "MaxTh", StringValue(redMaxTh),
                           "QueueLimit", UintegerValue(queueLimit),
                           "MeanPktSize", UintegerValue(pktSize),
                           "Wait", BooleanValue(true),
                           "Gentle", BooleanValue(true),
                           "UseHardDrop", BooleanValue(false),
                           "Mode", EnumValue(modeBytes ? QueueDisc::QUEUE_DISC_MODE_BYTES
                                                       : QueueDisc::QUEUE_DISC_MODE_PACKETS));
    }
  else
    {
      NS_ABORT_MSG("Unknown queue type: " << queueType);
    }

  // Install queue disc on the right router's incoming interface from the left router
  tch.Install(routerLink.Get(1)); // router node 1's device connected to router node 0

  // Setup OnOff application on right-side nodes
  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(routerNodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper client("ns3::TcpSocketFactory", Address());
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
  client.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  client.SetAttribute("PacketSize", UintegerValue(pktSize));

  for (uint32_t i = 0; i < nLeaf; ++i)
    {
      AddressValue remoteAddress(InetSocketAddress(routerInterface.GetAddress(1), port));
      client.SetAttribute("Remote", remoteAddress);
      ApplicationContainer app = client.Install(leafNodes.Get(i));
      app.Start(Seconds(1.0 + i * 0.1));
      app.Stop(Seconds(9.0));
    }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Collect drop statistics
  Ptr<QueueDisc> qd = StaticCast<PointToPointNetDevice>(routerLink.Get(1))->GetQueueDisc();
  if (qd)
    {
      totalDroppedPackets = qd->GetTotalDroppedPackets();
    }

  Simulator::Destroy();
}

void
DumbbellQueueDiscExperiment::Report()
{
  std::cout << "Total packets dropped in queue: " << totalDroppedPackets << std::endl;
}

int
main(int argc, char* argv[])
{
  uint32_t nLeaf = 5;
  std::string redMinTh = "5";
  std::string redMaxTh = "15";
  uint32_t pktSize = 1000;
  std::string dataRate = "1Mbps";
  std::string queueType = "RED";
  bool modeBytes = false;
  uint32_t queueLimit = 25;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nLeaf", "Number of left and right leaf nodes", nLeaf);
  cmd.AddValue("redMinTh", "RED queue minimum threshold (packets or bytes)", redMinTh);
  cmd.AddValue("redMaxTh", "RED queue maximum threshold (packets or bytes)", redMaxTh);
  cmd.AddValue("pktSize", "Packet size in bytes", pktSize);
  cmd.AddValue("dataRate", "Data rate for P2P links", dataRate);
  cmd.AddValue("queueType", "Queue disc type: RED or NLRED", queueType);
  cmd.AddValue("modeBytes", "Set queue mode to bytes", modeBytes);
  cmd.AddValue("queueLimit", "Queue limit in packets or bytes", queueLimit);
  cmd.Parse(argc, argv);

  DumbbellQueueDiscExperiment experiment;
  experiment.Run(nLeaf, redMinTh, redMaxTh, pktSize, dataRate, queueType, modeBytes, queueLimit);
  experiment.Report();

  return 0;
}