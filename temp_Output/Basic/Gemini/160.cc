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

  // Mesh Topology
  NodeContainer meshNodes;
  meshNodes.Create(4);

  PointToPointHelper meshPointToPoint;
  meshPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  meshPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer meshDevices[4];
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      NetDeviceContainer link = meshPointToPoint.Install(meshNodes.Get(i), meshNodes.Get(j));
      meshDevices[i].Add(link.Get(0));
      meshDevices[j].Add(link.Get(1));
    }
  }

  // Tree Topology
  NodeContainer treeNodes;
  treeNodes.Create(4);

  PointToPointHelper treePointToPoint;
  treePointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  treePointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer treeDevices[3];
  treeDevices[0] = treePointToPoint.Install(treeNodes.Get(0), treeNodes.Get(1));
  treeDevices[1] = treePointToPoint.Install(treeNodes.Get(0), treeNodes.Get(2));
  treeDevices[2] = treePointToPoint.Install(treeNodes.Get(2), treeNodes.Get(3));


  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(meshNodes);
  internet.Install(treeNodes);

  // Assign Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces[4];
    for (int i = 0; i < 4; ++i) {
        meshInterfaces[i] = ipv4.Assign(meshDevices[i]);
    }


  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces[3];
  treeInterfaces[0] = ipv4.Assign(treeDevices[0]);
  treeInterfaces[1] = ipv4.Assign(treeDevices[1]);
  treeInterfaces[2] = ipv4.Assign(treeDevices[2]);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP Traffic - Mesh
  UdpServerHelper meshUdpServer(9);
  ApplicationContainer meshServerApps = meshUdpServer.Install(meshNodes.Get(3));
  meshServerApps.Start(Seconds(1.0));
  meshServerApps.Stop(Seconds(20.0));

  UdpClientHelper meshUdpClient(meshInterfaces[3].GetAddress(1), 9);
  meshUdpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  meshUdpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  meshUdpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer meshClientApps = meshUdpClient.Install(meshNodes.Get(0));
  meshClientApps.Start(Seconds(2.0));
  meshClientApps.Stop(Seconds(20.0));

  // UDP Traffic - Tree
  UdpServerHelper treeUdpServer(9);
  ApplicationContainer treeServerApps = treeUdpServer.Install(treeNodes.Get(3));
  treeServerApps.Start(Seconds(1.0));
  treeServerApps.Stop(Seconds(20.0));

  UdpClientHelper treeUdpClient(treeInterfaces[2].GetAddress(2), 9);
  treeUdpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  treeUdpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  treeUdpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer treeClientApps = treeUdpClient.Install(treeNodes.Get(0));
  treeClientApps.Start(Seconds(2.0));
  treeClientApps.Stop(Seconds(20.0));

  // Mobility - Linear movements for visualiztion
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(meshNodes);
  mobility.Install(treeNodes);

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run Simulation
  Simulator::Stop(Seconds(20.0));

  AnimationInterface anim("mesh-tree.xml");
  anim.EnablePacketMetadata(true);

  Simulator::Run();

  // Print Flow Monitor Statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double meshTxBytes = 0, meshRxBytes = 0;
  double treeTxBytes = 0, treeRxBytes = 0;

  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
    if (t.destinationAddress == meshInterfaces[3].GetAddress(1)) {
        meshTxBytes += iter->second.txBytes;
        meshRxBytes += iter->second.rxBytes;
    }
     if (t.destinationAddress == treeInterfaces[2].GetAddress(2)) {
        treeTxBytes += iter->second.txBytes;
        treeRxBytes += iter->second.rxBytes;
    }
  }
    double simulationTimeSeconds = 20.0 - 2.0;

    double meshThroughput = (meshRxBytes * 8.0) / (simulationTimeSeconds * 1000000.0);
    double treeThroughput = (treeRxBytes * 8.0) / (simulationTimeSeconds * 1000000.0);

    std::cout << "Mesh Throughput: " << meshThroughput << " Mbps" << std::endl;
    std::cout << "Tree Throughput: " << treeThroughput << " Mbps" << std::endl;
    std::cout << "Simulation over" << std::endl;

  Simulator::Destroy();
  return 0;
}