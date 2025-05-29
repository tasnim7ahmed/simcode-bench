#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t k = 4;
  uint32_t numPods = k;
  uint32_t numCoreRouters = (k / 2) * (k / 2);
  uint32_t numAggregateRouters = numPods * (k / 2);
  uint32_t numEdgeRouters = numPods * (k / 2);
  uint32_t numServers = numPods * (k / 2) * (k / 2);

  NodeContainer coreRouters, aggregateRouters, edgeRouters, servers;
  coreRouters.Create(numCoreRouters);
  aggregateRouters.Create(numAggregateRouters);
  edgeRouters.Create(numEdgeRouters);
  servers.Create(numServers);

  InternetStackHelper internet;
  internet.Install(coreRouters);
  internet.Install(aggregateRouters);
  internet.Install(edgeRouters);
  internet.Install(servers);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1us"));

  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;

  uint32_t routerIndex = 0;

  for (uint32_t pod = 0; pod < numPods; ++pod) {
    for (uint32_t agg = 0; agg < k / 2; ++agg) {
      for (uint32_t core = 0; core < k / 2; ++core) {
        devices = pointToPoint.Install(coreRouters.Get(core * (k / 2) + agg),
                                      aggregateRouters.Get(pod * (k / 2) + agg));
        interfaces.Add(internet.AssignIpv4Addresses(devices, Ipv4Address("10." + std::to_string(pod + 10) + "." + std::to_string(agg + core + 1) + ".0"), Ipv4Mask("255.255.255.252")));
      }
    }
  }

  for (uint32_t pod = 0; pod < numPods; ++pod) {
    for (uint32_t edge = 0; edge < k / 2; ++edge) {
      for (uint32_t agg = 0; agg < k / 2; ++agg) {
        devices = pointToPoint.Install(aggregateRouters.Get(pod * (k / 2) + agg),
                                      edgeRouters.Get(pod * (k / 2) + edge));
        interfaces.Add(internet.AssignIpv4Addresses(devices, Ipv4Address("10." + std::to_string(pod + 20) + "." + std::to_string(edge + agg + 1) + ".0"), Ipv4Mask("255.255.255.252")));
      }
    }
  }

  for (uint32_t pod = 0; pod < numPods; ++pod) {
    for (uint32_t server = 0; server < k / 2; ++server) {
      for (uint32_t edge = 0; edge < k / 2; ++edge) {
        devices = pointToPoint.Install(edgeRouters.Get(pod * (k / 2) + edge),
                                      servers.Get(pod * (k / 2) * (k / 2) + server * (k / 2) + edge));
        interfaces.Add(internet.AssignIpv4Addresses(devices, Ipv4Address("10." + std::to_string(pod + 30) + "." + std::to_string(server * (k/2) + edge + 1) + ".0"), Ipv4Mask("255.255.255.252")));
      }
    }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory",
                        InetSocketAddress(interfaces.GetAddress(numCoreRouters*2 + numAggregateRouters*2 + numEdgeRouters*2), port));
  source.SetAttribute("SendSize", UintegerValue(1024));
  source.SetAttribute("MaxBytes", UintegerValue(100000000));

  ApplicationContainer sourceApps = source.Install(servers.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(servers.Get(numServers-1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  AnimationInterface anim("fat-tree.xml");

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
  }

  Simulator::Destroy();
  return 0;
}