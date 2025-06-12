#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create nodes for each topology
  NodeContainer ringNodes, meshNodes, busNodes, lineNodes, starNodes, treeNodes;

  // Ring Topology (4 nodes)
  ringNodes.Create(4);

  // Mesh Topology (4 nodes)
  meshNodes.Create(4);

  // Bus Topology (3 nodes)
  busNodes.Create(3);

  // Line Topology (4 nodes)
  lineNodes.Create(4);

  // Star Topology (1 central node, 3 spoke nodes)
  starNodes.Create(4);

  // Tree Topology (1 root, 2 children, each child has 1 child)
  treeNodes.Create(5);

  // Create point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install Internet stack on all nodes
  InternetStackHelper internet;
  internet.Install(ringNodes);
  internet.Install(meshNodes);
  internet.Install(busNodes);
  internet.Install(lineNodes);
  internet.Install(starNodes);
  internet.Install(treeNodes);

  // Ring Topology Connections
  NetDeviceContainer ringDevices[4];
  for (int i = 0; i < 4; ++i) {
    ringDevices[i] = pointToPoint.Install(NodeContainer(ringNodes.Get(i), ringNodes.Get((i + 1) % 4)));
  }

  // Mesh Topology Connections (Complete Graph)
  NetDeviceContainer meshDevices[6];
  int deviceIndex = 0;
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      meshDevices[deviceIndex++] = pointToPoint.Install(NodeContainer(meshNodes.Get(i), meshNodes.Get(j)));
    }
  }

  // Bus Topology Connections (using csma)
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));
  NetDeviceContainer busDevices = csma.Install(busNodes);

  // Line Topology Connections
  NetDeviceContainer lineDevices[3];
  for (int i = 0; i < 3; ++i) {
    lineDevices[i] = pointToPoint.Install(NodeContainer(lineNodes.Get(i), lineNodes.Get(i + 1)));
  }

  // Star Topology Connections (Central node is starNodes.Get(0))
  NetDeviceContainer starDevices[3];
  for (int i = 1; i < 4; ++i) {
    starDevices[i - 1] = pointToPoint.Install(NodeContainer(starNodes.Get(0), starNodes.Get(i)));
  }

  // Tree Topology Connections (Root is treeNodes.Get(0))
  NetDeviceContainer treeDevices[4];
  treeDevices[0] = pointToPoint.Install(NodeContainer(treeNodes.Get(0), treeNodes.Get(1)));
  treeDevices[1] = pointToPoint.Install(NodeContainer(treeNodes.Get(0), treeNodes.Get(2)));
  treeDevices[2] = pointToPoint.Install(NodeContainer(treeNodes.Get(1), treeNodes.Get(3)));
  treeDevices[3] = pointToPoint.Install(NodeContainer(treeNodes.Get(2), treeNodes.Get(4)));

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces = address.Assign(ringDevices[0]);
  address.NewNetwork();
  Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices[0]);
  address.NewNetwork();
  Ipv4InterfaceContainer busInterfaces = address.Assign(busDevices);
  address.NewNetwork();
  Ipv4InterfaceContainer lineInterfaces = address.Assign(lineDevices[0]);
  address.NewNetwork();
  Ipv4InterfaceContainer starInterfaces = address.Assign(starDevices[0]);
  address.NewNetwork();
  Ipv4InterfaceContainer treeInterfaces = address.Assign(treeDevices[0]);

  // Connect the different Topologies.  Connect Ring to Mesh, Bus to Line, Star to Tree.

  NetDeviceContainer ringMeshConnect = pointToPoint.Install(NodeContainer(ringNodes.Get(0), meshNodes.Get(0)));
  address.NewNetwork();
  Ipv4InterfaceContainer ringMeshInterfaces = address.Assign(ringMeshConnect);

  NetDeviceContainer busLineConnect = pointToPoint.Install(NodeContainer(busNodes.Get(0), lineNodes.Get(0)));
  address.NewNetwork();
  Ipv4InterfaceContainer busLineInterfaces = address.Assign(busLineConnect);

  NetDeviceContainer starTreeConnect = pointToPoint.Install(NodeContainer(starNodes.Get(0), treeNodes.Get(0)));
  address.NewNetwork();
  Ipv4InterfaceContainer starTreeInterfaces = address.Assign(starTreeConnect);

  // Enable OSPF
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create Applications (e.g., BulkSendApplication for traffic)
  uint16_t port = 9;
  BulkSendHelper source("ns3::TcpSocketFactory",
                         InetSocketAddress(starTreeInterfaces.GetAddress(1), port));
  source.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer sourceApps = source.Install(starNodes.Get(3));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(treeNodes.Get(0));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  // Enable tracing
  pointToPoint.EnablePcapAll("hybrid");

  // Enable NetAnim
  AnimationInterface anim("hybrid.xml");
  anim.SetConstantPosition(ringNodes.Get(0), 10, 10);
  anim.SetConstantPosition(ringNodes.Get(1), 20, 10);
  anim.SetConstantPosition(ringNodes.Get(2), 30, 10);
  anim.SetConstantPosition(ringNodes.Get(3), 40, 10);

  anim.SetConstantPosition(meshNodes.Get(0), 10, 20);
  anim.SetConstantPosition(meshNodes.Get(1), 20, 20);
  anim.SetConstantPosition(meshNodes.Get(2), 30, 20);
  anim.SetConstantPosition(meshNodes.Get(3), 40, 20);

  anim.SetConstantPosition(busNodes.Get(0), 10, 30);
  anim.SetConstantPosition(busNodes.Get(1), 20, 30);
  anim.SetConstantPosition(busNodes.Get(2), 30, 30);

  anim.SetConstantPosition(lineNodes.Get(0), 10, 40);
  anim.SetConstantPosition(lineNodes.Get(1), 20, 40);
  anim.SetConstantPosition(lineNodes.Get(2), 30, 40);
  anim.SetConstantPosition(lineNodes.Get(3), 40, 40);

  anim.SetConstantPosition(starNodes.Get(0), 60, 25);
  anim.SetConstantPosition(starNodes.Get(1), 50, 20);
  anim.SetConstantPosition(starNodes.Get(2), 70, 20);
  anim.SetConstantPosition(starNodes.Get(3), 60, 15);

  anim.SetConstantPosition(treeNodes.Get(0), 80, 25);
  anim.SetConstantPosition(treeNodes.Get(1), 70, 30);
  anim.SetConstantPosition(treeNodes.Get(2), 90, 30);
  anim.SetConstantPosition(treeNodes.Get(3), 70, 35);
  anim.SetConstantPosition(treeNodes.Get(4), 90, 35);

  // Run the simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}