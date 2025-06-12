#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nClients = 5;
  double simulationTime = 10.0;

  NodeContainer nodes;
  nodes.Create(nClients + 1); // 1 server + nClients

  NodeContainer serverNode = NodeContainer(nodes.Get(0));
  NodeContainer clientNodes;
  for (uint32_t i = 1; i <= nClients; ++i) {
    clientNodes.Add(nodes.Get(i));
  }

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  for (uint32_t i = 1; i <= nClients; ++i) {
    NetDeviceContainer link = pointToPoint.Install(serverNode.Get(0), clientNodes.Get(i - 1));
    devices.Add(link.Get(0));
    devices.Add(link.Get(1));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;

  for (uint32_t i = 0; i < nClients + 1; ++i) {
    interfaces.Add(address.AssignOne(devices.Get(2 * i)));
    interfaces.Add(address.AssignOne(devices.Get(2 * i + 1)));
    address.NewNetwork();
  }
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
  source.SetAttribute("SendSize", UintegerValue(1400));
  source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data

  ApplicationContainer sourceApps;
  for(uint32_t i = 1; i <= nClients; ++i) {
    sourceApps.Add(source.Install(clientNodes.Get(i - 1)));
  }
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(simulationTime - 1.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(serverNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime));

  AnimationInterface anim("star-network.xml");
  anim.SetConstantPosition(serverNode.Get(0), 10, 10);
  for (uint32_t i = 0; i < nClients; ++i) {
    anim.SetConstantPosition(clientNodes.Get(i), 20 + i * 5, 20);
  }
  
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
    std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Loss Ratio: " << (double)i->second.lostPackets / i->second.txPackets << "\n";
  }
  monitor->SerializeToXmlFile("star-network.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}