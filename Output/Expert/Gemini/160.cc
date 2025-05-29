#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/tree-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  uint32_t numNodes = 4;

  NodeContainer meshNodes;
  meshNodes.Create(numNodes);

  NodeContainer treeNodes;
  treeNodes.Create(numNodes);

  InternetStackHelper stackHelper;
  stackHelper.Install(meshNodes);
  stackHelper.Install(treeNodes);

  Ipv4AddressHelper addressHelper;
  addressHelper.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces = addressHelper.Assign(meshNodes);

  addressHelper.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces = addressHelper.Assign(treeNodes);

  MeshHelper meshHelper;
  meshHelper.SetStackInstaller(stackHelper);
  meshHelper.Set распространение("DlDelay", StringValue("2ms"));
  meshHelper.Set распространение("DataRate", StringValue("5Mbps"));
  NetDeviceContainer meshDevices = meshHelper.Install(meshNodes);

  PointToPointHelper pointToPointHelper;
  pointToPointHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPointHelper.SetChannelAttribute("Delay", StringValue("2ms"));

  TreeHelper treeHelper;
  treeHelper.SetPointToPointHelper(pointToPointHelper);
  NetDeviceContainer treeDevices = treeHelper.Install(treeNodes);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpServerHelper server(port);

  ApplicationContainer serverAppsMesh = server.Install(meshNodes.Get(numNodes - 1));
  serverAppsMesh.Start(Seconds(1.0));
  serverAppsMesh.Stop(Seconds(20.0));

  ApplicationContainer serverAppsTree = server.Install(treeNodes.Get(numNodes - 1));
  serverAppsTree.Start(Seconds(1.0));
  serverAppsTree.Stop(Seconds(20.0));


  UdpClientHelper clientMesh(meshInterfaces.GetAddress(numNodes - 1), port);
  clientMesh.SetAttribute("MaxPackets", UintegerValue(4294967295));
  clientMesh.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientMesh.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientAppsMesh = clientMesh.Install(meshNodes.Get(0));
  clientAppsMesh.Start(Seconds(2.0));
  clientAppsMesh.Stop(Seconds(20.0));

  UdpClientHelper clientTree(treeInterfaces.GetAddress(numNodes - 1), port);
  clientTree.SetAttribute("MaxPackets", UintegerValue(4294967295));
  clientTree.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientTree.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientAppsTree = clientTree.Install(treeNodes.Get(0));
  clientAppsTree.Start(Seconds(2.0));
  clientAppsTree.Stop(Seconds(20.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  AnimationInterface anim("mesh-tree.xml");
  anim.SetConstantPosition(meshNodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(meshNodes.Get(1), 20.0, 10.0);
  anim.SetConstantPosition(meshNodes.Get(2), 10.0, 20.0);
  anim.SetConstantPosition(meshNodes.Get(3), 20.0, 20.0);

  anim.SetConstantPosition(treeNodes.Get(0), 40.0, 10.0);
  anim.SetConstantPosition(treeNodes.Get(1), 50.0, 10.0);
  anim.SetConstantPosition(treeNodes.Get(2), 40.0, 20.0);
  anim.SetConstantPosition(treeNodes.Get(3), 50.0, 20.0);

  Simulator::Stop(Seconds(20.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    std::cout << "  Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
  }

  monitor->SerializeToXmlFile("mesh-tree.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}