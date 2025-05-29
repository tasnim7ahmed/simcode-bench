#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t k = 4; // Radix of the Fat-Tree
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer coreNodes;
  NodeContainer aggregationNodes;
  NodeContainer edgeNodes;
  NodeContainer hostNodes;

  coreNodes.Create(k * k / 4);
  aggregationNodes.Create(k * k / 2);
  edgeNodes.Create(k * k / 2);
  hostNodes.Create(k * k);

  InternetStackHelper stack;
  stack.Install(coreNodes);
  stack.Install(aggregationNodes);
  stack.Install(edgeNodes);
  stack.Install(hostNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));

  // Core to Aggregation
  uint32_t aggNodeIdx = 0;
  for (uint32_t pod = 0; pod < k; ++pod) {
    for (uint32_t core = 0; core < k / 2; ++core) {
      for (uint32_t i = 0; i < k / 2; ++i) {
        NetDeviceContainer link = pointToPoint.Install(coreNodes.Get(core * k / 2 + i), aggregationNodes.Get(pod * k / 2 + aggNodeIdx));
      }
      aggNodeIdx++;
    }
    aggNodeIdx = 0;
  }

  // Aggregation to Edge
  uint32_t edgeNodeIdx = 0;
  for (uint32_t pod = 0; pod < k; ++pod) {
    for (uint32_t agg = 0; agg < k / 2; ++agg) {
      for (uint32_t i = 0; i < k / 2; ++i) {
        NetDeviceContainer link = pointToPoint.Install(aggregationNodes.Get(pod * k / 2 + agg), edgeNodes.Get(pod * k / 2 + edgeNodeIdx));
        edgeNodeIdx++;
      }
      edgeNodeIdx = 0;
    }
  }

  // Edge to Host
  uint32_t hostNodeIdx = 0;
  for (uint32_t pod = 0; pod < k; ++pod) {
    for (uint32_t edge = 0; edge < k / 2; ++edge) {
      for (uint32_t i = 0; i < k / 2; ++i) {
        NetDeviceContainer link = pointToPoint.Install(edgeNodes.Get(pod * k / 2 + edge), hostNodes.Get(pod * k + hostNodeIdx));
        hostNodeIdx++;
      }
      hostNodeIdx = 0;
    }
  }

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDevice::GetGlobal());
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer apps = server.Install(hostNodes.Get(k*k - 1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(k*k - 1), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Time("1us")));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  apps = client.Install(hostNodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  AnimationInterface anim("fat-tree.xml");
  anim.EnablePacketMetadata(true);

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}