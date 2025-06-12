#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  int nLeftLeaf = 3;
  int nRightLeaf = 3;
  std::string queueType = "RED";
  uint32_t maxPackets = 20;
  uint32_t packetSize = 1024;
  std::string dataRate = "10Mbps";
  std::string delay = "2ms";
  double redMeanPktSize = 1000;
  double redQbWeight = 0.002;
  double redMinTh = 5;
  double redMaxTh = 15;
  double fengAlpha = 0.5;
  double fengBeta = 0.5;

  cmd.AddValue("nLeftLeaf", "Number of left side leaf nodes", nLeftLeaf);
  cmd.AddValue("nRightLeaf", "Number of right side leaf nodes", nRightLeaf);
  cmd.AddValue("queueType", "Queue type (RED or AdaptiveRED)", queueType);
  cmd.AddValue("maxPackets", "Max packets in queue", maxPackets);
  cmd.AddValue("packetSize", "Size of packets", packetSize);
  cmd.AddValue("dataRate", "Data rate of links", dataRate);
  cmd.AddValue("delay", "Delay of links", delay);
  cmd.AddValue("redMeanPktSize", "RED mean packet size", redMeanPktSize);
  cmd.AddValue("redQbWeight", "RED Qb weight", redQbWeight);
  cmd.AddValue("redMinTh", "RED min threshold", redMinTh);
  cmd.AddValue("redMaxTh", "RED max threshold", redMaxTh);
  cmd.AddValue("fengAlpha", "Feng's Adaptive RED alpha", fengAlpha);
  cmd.AddValue("fengBeta", "Feng's Adaptive RED beta", fengBeta);

  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer leftRouters, rightRouters, leftLeafNodes, rightLeafNodes;
  leftRouters.Create(1);
  rightRouters.Create(1);
  leftLeafNodes.Create(nLeftLeaf);
  rightLeafNodes.Create(nRightLeaf);

  PointToPointHelper leafLink;
  leafLink.SetDeviceAttribute("DataRate", StringValue(dataRate));
  leafLink.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer leftLeafDevices[nLeftLeaf];
  NetDeviceContainer rightLeafDevices[nRightLeaf];

  for (int i = 0; i < nLeftLeaf; ++i) {
    NodeContainer leafRouter = NodeContainer(leftLeafNodes.Get(i), leftRouters.Get(0));
    leftLeafDevices[i] = leafLink.Install(leafRouter);
  }

  for (int i = 0; i < nRightLeaf; ++i) {
    NodeContainer leafRouter = NodeContainer(rightLeafNodes.Get(i), rightRouters.Get(0));
    rightLeafDevices[i] = leafLink.Install(leafRouter);
  }

  PointToPointHelper routerLink;
  routerLink.SetDeviceAttribute("DataRate", StringValue(dataRate));
  routerLink.SetChannelAttribute("Delay", StringValue(delay));

  NetDeviceContainer routerDevices = routerLink.Install(leftRouters.Get(0), rightRouters.Get(0));

  InternetStackHelper stack;
  stack.Install(leftRouters);
  stack.Install(rightRouters);
  stack.Install(leftLeafNodes);
  stack.Install(rightLeafNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftRouterInterfaces[nLeftLeaf];
  for (int i = 0; i < nLeftLeaf; ++i) {
    leftRouterInterfaces[i] = address.Assign(leftLeafDevices[i]);
    address.NewNetwork();
  }

  Ipv4InterfaceContainer rightRouterInterfaces[nRightLeaf];
  for (int i = 0; i < nRightLeaf; ++i) {
    rightRouterInterfaces[i] = address.Assign(rightLeafDevices[i]);
    address.NewNetwork();
  }

  Ipv4InterfaceContainer routerInterfaces = address.Assign(routerDevices);
  address.NewNetwork();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  TrafficControlLayer::SetRootQueueDisc("ns3::FifoQueueDisc",
                                         "MaxSize", StringValue(std::to_string(maxPackets) + "p"));

  QueueDiscContainer queueDiscs;
  if (queueType == "RED") {
     TrafficControlHelper tch;
     QueueDiscTypeId redQueueDiscType = TypeId::LookupByName ("ns3::RedQueueDisc");
     AttributeValue meanPktSize = DoubleValue (redMeanPktSize);
     AttributeValue qbWeight = DoubleValue (redQbWeight);
     AttributeValue minTh = DoubleValue (redMinTh * packetSize);
     AttributeValue maxTh = DoubleValue (redMaxTh * packetSize);

     tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                            "LinkBandwidth", StringValue (dataRate),
                            "LinkDelay", StringValue (delay));

    for (int i = 0; i < nLeftLeaf; ++i) {
      queueDiscs.Add(tch.Install(routerDevices.Get(0)));
      queueDiscs.Add(tch.Install(leftLeafDevices[i].Get(1)));
    }
    for (int i = 0; i < nRightLeaf; ++i) {
        queueDiscs.Add(tch.Install(routerDevices.Get(1)));
        queueDiscs.Add(tch.Install(rightLeafDevices[i].Get(1)));
    }
  } else if (queueType == "AdaptiveRED") {
    TrafficControlHelper tch;
     QueueDiscTypeId adaptiveRedQueueDiscType = TypeId::LookupByName ("ns3::AdaptiveRedQueueDisc");
     AttributeValue alpha = DoubleValue (fengAlpha);
     AttributeValue beta = DoubleValue (fengBeta);

     tch.SetRootQueueDisc ("ns3::AdaptiveRedQueueDisc",
                            "LinkBandwidth", StringValue (dataRate),
                            "LinkDelay", StringValue (delay));

        for (int i = 0; i < nLeftLeaf; ++i) {
          queueDiscs.Add(tch.Install(routerDevices.Get(0)));
          queueDiscs.Add(tch.Install(leftLeafDevices[i].Get(1)));
        }
        for (int i = 0; i < nRightLeaf; ++i) {
            queueDiscs.Add(tch.Install(routerDevices.Get(1)));
            queueDiscs.Add(tch.Install(rightLeafDevices[i].Get(1)));
        }


  } else {
    std::cerr << "Invalid queue type: " << queueType << std::endl;
    return 1;
  }

  OnOffHelper onoff("ns3::TcpSocketFactory", Address(InetSocketAddress(routerInterfaces.GetAddress(0), 5000)));
  onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=1|Max=2]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=1|Max=2]"));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("DataRate", StringValue(dataRate));

  ApplicationContainer clientApps[nRightLeaf];
  for (int i = 0; i < nRightLeaf; ++i) {
      clientApps[i] = onoff.Install(rightLeafNodes.Get(i));
      clientApps[i].Start(Seconds(1.0));
      clientApps[i].Stop(Seconds(10.0));
  }

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 5000));
  ApplicationContainer sinkApp = sink.Install(leftRouters.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  for (uint32_t i = 0; i < queueDiscs.GetN(); ++i) {
        Ptr<QueueDisc> queueDisc = queueDiscs.Get(i);
        if (queueDisc != nullptr) {
            std::cout << "QueueDisc " << i << " stats:" << std::endl;
            queueDisc->PrintStats();
        }
  }

  Simulator::Destroy();
  return 0;
}