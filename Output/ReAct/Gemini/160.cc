#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Mesh Network
  NodeContainer meshNodes;
  meshNodes.Create(4);

  InternetStackHelper internetMesh;
  internetMesh.Install(meshNodes);

  PointToPointHelper p2pMesh;
  p2pMesh.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2pMesh.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer meshDevices[4];
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      NetDeviceContainer link = p2pMesh.Install(meshNodes.Get(i), meshNodes.Get(j));
      meshDevices[i].Add(link.Get(0));
      meshDevices[j].Add(link.Get(1));
    }
  }

  Ipv4AddressHelper addressMesh;
  addressMesh.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces[4];
    int k = 0;
  for (int i = 0; i < 4; ++i) {
      NetDeviceContainer temp;
      for(uint32_t j =0; j < meshDevices[i].GetN(); ++j){
          temp.Add(meshDevices[i].Get(j));
      }
    meshInterfaces[i] = addressMesh.Assign(temp);
    addressMesh.NewNetwork();
    k+= meshDevices[i].GetN();
  }

  // Tree Network
  NodeContainer treeNodes;
  treeNodes.Create(4);

  InternetStackHelper internetTree;
  internetTree.Install(treeNodes);

  PointToPointHelper p2pTree;
  p2pTree.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2pTree.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer treeDevices[3];
  treeDevices[0] = p2pTree.Install(treeNodes.Get(0), treeNodes.Get(1));
  treeDevices[1] = p2pTree.Install(treeNodes.Get(0), treeNodes.Get(2));
  treeDevices[2] = p2pTree.Install(treeNodes.Get(2), treeNodes.Get(3));

  Ipv4AddressHelper addressTree;
  addressTree.SetBase("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces[3];
  treeInterfaces[0] = addressTree.Assign(treeDevices[0]);
  addressTree.NewNetwork();
  treeInterfaces[1] = addressTree.Assign(treeDevices[1]);
  addressTree.NewNetwork();
  treeInterfaces[2] = addressTree.Assign(treeDevices[2]);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP Applications
  uint16_t port = 9;

  UdpServerHelper serverMesh(port);
  ApplicationContainer serverAppsMesh = serverMesh.Install(meshNodes.Get(3));
  serverAppsMesh.Start(Seconds(1.0));
  serverAppsMesh.Stop(Seconds(20.0));

  UdpClientHelper clientMesh(meshInterfaces[3].GetAddress(0), port);
  clientMesh.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientMesh.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  ApplicationContainer clientAppsMesh = clientMesh.Install(meshNodes.Get(0));
  clientAppsMesh.Start(Seconds(2.0));
  clientAppsMesh.Stop(Seconds(20.0));

  UdpServerHelper serverTree(port);
  ApplicationContainer serverAppsTree = serverTree.Install(treeNodes.Get(3));
  serverAppsTree.Start(Seconds(1.0));
  serverAppsTree.Stop(Seconds(20.0));

  UdpClientHelper clientTree(treeInterfaces[2].GetAddress(1), port);
  clientTree.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientTree.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  ApplicationContainer clientAppsTree = clientTree.Install(treeNodes.Get(0));
  clientAppsTree.Start(Seconds(2.0));
  clientAppsTree.Stop(Seconds(20.0));

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("0.0"),
                                  "Y", StringValue("0.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("10x10"));
  mobility.Install(meshNodes);
  mobility.Install(treeNodes);

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Animation
  AnimationInterface anim("mesh-tree-animation.xml");
  anim.SetConstantPosition(meshNodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(meshNodes.Get(1), 20.0, 10.0);
  anim.SetConstantPosition(meshNodes.Get(2), 10.0, 20.0);
  anim.SetConstantPosition(meshNodes.Get(3), 20.0, 20.0);
  anim.SetConstantPosition(treeNodes.Get(0), 30.0, 10.0);
  anim.SetConstantPosition(treeNodes.Get(1), 40.0, 10.0);
  anim.SetConstantPosition(treeNodes.Get(2), 30.0, 20.0);
  anim.SetConstantPosition(treeNodes.Get(3), 40.0, 20.0);

  // Simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  // Analysis
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalThroughputMesh = 0.0;
  double totalDelayMesh = 0.0;
  double totalPacketsMesh = 0.0;

  double totalThroughputTree = 0.0;
  double totalDelayTree = 0.0;
  double totalPacketsTree = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

    if (t.destinationAddress == meshInterfaces[3].GetAddress(0)) {
      totalThroughputMesh += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000.0;
      totalDelayMesh += i->second.delaySum.GetSeconds();
      totalPacketsMesh += i->second.txPackets;

    } else if (t.destinationAddress == treeInterfaces[2].GetAddress(1)){
        totalThroughputTree += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000.0;
        totalDelayTree += i->second.delaySum.GetSeconds();
        totalPacketsTree += i->second.txPackets;
    }
  }

  std::cout << "***Mesh Network Results***" << std::endl;
  std::cout << "  Total Throughput: " << totalThroughputMesh << " Mbps" << std::endl;
  if(totalPacketsMesh > 0){
     std::cout << "  Average Delay: " << totalDelayMesh / totalPacketsMesh << " s" << std::endl;
  } else {
      std::cout << "  Average Delay: N/A s" << std::endl;
  }

  std::cout << "***Tree Network Results***" << std::endl;
  std::cout << "  Total Throughput: " << totalThroughputTree << " Mbps" << std::endl;
   if(totalPacketsTree > 0){
     std::cout << "  Average Delay: " << totalDelayTree / totalPacketsTree << " s" << std::endl;
  } else {
      std::cout << "  Average Delay: N/A s" << std::endl;
  }

  Simulator::Destroy();
  return 0;
}