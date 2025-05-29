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

  NodeContainer nodes;
  nodes.Create(6);

  NodeContainer serverNode = nodes.Get(0);
  NodeContainer clientNodes;
  for (uint32_t i = 1; i < 6; ++i) {
    clientNodes.Add(nodes.Get(i));
  }

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer serverDevices, clientDevices[5];
  for (uint32_t i = 1; i < 6; ++i) {
    NodeContainer linkNodes;
    linkNodes.Add(nodes.Get(0));
    linkNodes.Add(nodes.Get(i));

    NetDeviceContainer linkDevices = pointToPoint.Install(linkNodes);
    serverDevices.Add(linkDevices.Get(0));
    clientDevices[i - 1].Add(linkDevices.Get(1));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer serverInterfaces, clientInterfaces[5];
  for (uint32_t i = 0; i < 5; ++i) {
    Ipv4InterfaceContainer linkInterfaces = address.Assign(NetDeviceContainer(serverDevices.Get(i), clientDevices[i]));
    serverInterfaces.Add(linkInterfaces.Get(0));
    clientInterfaces[i].Add(linkInterfaces.Get(1));
    address.NewNetwork();
  }

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(serverNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(serverInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[5];
  for(uint32_t i = 0; i < 5; ++i) {
      clientApps[i] = echoClient.Install(clientNodes.Get(i));
      clientApps[i].Start(Seconds(2.0 + i * 0.1));
      clientApps[i].Stop(Seconds(10.0));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(11.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  AnimationInterface anim("star_topology.xml");
  anim.SetConstantPosition(serverNode, 0.0, 0.0);
  for (uint32_t i = 0; i < 5; ++i) {
      anim.SetConstantPosition(clientNodes.Get(i), 5.0 * (i + 1), 5.0 * (i + 1));
  }

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
	  std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	  std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	  std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
	  std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
	  std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";

  }

  monitor->SerializeToXmlFile("star_topology.flowmon", true, true);

  Simulator::Destroy();
  return 0;
}