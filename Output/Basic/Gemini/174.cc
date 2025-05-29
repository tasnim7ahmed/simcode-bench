#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-ospf-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopology");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetAttribute("OspfRouting", "TraceLevel", StringValue("All"));

  NodeContainer ringNodes;
  ringNodes.Create(4);

  NodeContainer busNodes;
  busNodes.Create(3);

  NodeContainer lineNodes;
  lineNodes.Create(3);

  NodeContainer starNode;
  starNode.Create(1);

  NodeContainer starLeafNodes;
  starLeafNodes.Create(3);

  NodeContainer meshNodes;
  meshNodes.Create(4);

  NodeContainer treeRoot;
  treeRoot.Create(1);

  NodeContainer treeLeaves;
  treeLeaves.Create(2);

  InternetStackHelper internet;
  internet.Install(ringNodes);
  internet.Install(busNodes);
  internet.Install(lineNodes);
  internet.Install(starNode);
  internet.Install(starLeafNodes);
  internet.Install(meshNodes);
  internet.Install(treeRoot);
  internet.Install(treeLeaves);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Ring Topology
  NetDeviceContainer ringDevices1 = p2p.Install(ringNodes.Get(0), ringNodes.Get(1));
  NetDeviceContainer ringDevices2 = p2p.Install(ringNodes.Get(1), ringNodes.Get(2));
  NetDeviceContainer ringDevices3 = p2p.Install(ringNodes.Get(2), ringNodes.Get(3));
  NetDeviceContainer ringDevices4 = p2p.Install(ringNodes.Get(3), ringNodes.Get(0));

  // Bus Topology
  PointToPointHelper busP2p;
  busP2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  busP2p.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer busDevices1 = busP2p.Install(busNodes.Get(0), busNodes.Get(1));
  NetDeviceContainer busDevices2 = busP2p.Install(busNodes.Get(1), busNodes.Get(2));

  // Line Topology
  NetDeviceContainer lineDevices1 = p2p.Install(lineNodes.Get(0), lineNodes.Get(1));
  NetDeviceContainer lineDevices2 = p2p.Install(lineNodes.Get(1), lineNodes.Get(2));

  // Star Topology
  NetDeviceContainer starDevices1 = p2p.Install(starNode.Get(0), starLeafNodes.Get(0));
  NetDeviceContainer starDevices2 = p2p.Install(starNode.Get(0), starLeafNodes.Get(1));
  NetDeviceContainer starDevices3 = p2p.Install(starNode.Get(0), starLeafNodes.Get(2));

  // Mesh Topology
  NetDeviceContainer meshDevices1 = p2p.Install(meshNodes.Get(0), meshNodes.Get(1));
  NetDeviceContainer meshDevices2 = p2p.Install(meshNodes.Get(0), meshNodes.Get(2));
  NetDeviceContainer meshDevices3 = p2p.Install(meshNodes.Get(0), meshNodes.Get(3));
  NetDeviceContainer meshDevices4 = p2p.Install(meshNodes.Get(1), meshNodes.Get(2));
  NetDeviceContainer meshDevices5 = p2p.Install(meshNodes.Get(1), meshNodes.Get(3));
  NetDeviceContainer meshDevices6 = p2p.Install(meshNodes.Get(2), meshNodes.Get(3));

  // Tree Topology
  NetDeviceContainer treeDevices1 = p2p.Install(treeRoot.Get(0), treeLeaves.Get(0));
  NetDeviceContainer treeDevices2 = p2p.Install(treeRoot.Get(0), treeLeaves.Get(1));

  // Interconnect Topologies
  NetDeviceContainer connectRingBus = p2p.Install(ringNodes.Get(0), busNodes.Get(0));
  NetDeviceContainer connectLineStar = p2p.Install(lineNodes.Get(0), starNode.Get(0));
  NetDeviceContainer connectMeshTree = p2p.Install(meshNodes.Get(0), treeRoot.Get(0));

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces1 = address.Assign(ringDevices1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces2 = address.Assign(ringDevices2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces3 = address.Assign(ringDevices3);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces4 = address.Assign(ringDevices4);

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer busInterfaces1 = address.Assign(busDevices1);

  address.SetBase("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer busInterfaces2 = address.Assign(busDevices2);

  address.SetBase("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer lineInterfaces1 = address.Assign(lineDevices1);

  address.SetBase("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer lineInterfaces2 = address.Assign(lineDevices2);

  address.SetBase("10.1.9.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces1 = address.Assign(starDevices1);

  address.SetBase("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces2 = address.Assign(starDevices2);

  address.SetBase("10.1.11.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces3 = address.Assign(starDevices3);

  address.SetBase("10.1.12.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces1 = address.Assign(meshDevices1);

  address.SetBase("10.1.13.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces2 = address.Assign(meshDevices2);

  address.SetBase("10.1.14.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces3 = address.Assign(meshDevices3);

  address.SetBase("10.1.15.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces4 = address.Assign(meshDevices4);

  address.SetBase("10.1.16.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces5 = address.Assign(meshDevices5);

  address.SetBase("10.1.17.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces6 = address.Assign(meshDevices6);

  address.SetBase("10.1.18.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces1 = address.Assign(treeDevices1);

  address.SetBase("10.1.19.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces2 = address.Assign(treeDevices2);

  address.SetBase("10.1.20.0", "255.255.255.0");
  Ipv4InterfaceContainer ringBusInterface = address.Assign(connectRingBus);

  address.SetBase("10.1.21.0", "255.255.255.0");
  Ipv4InterfaceContainer lineStarInterface = address.Assign(connectLineStar);

  address.SetBase("10.1.22.0", "255.255.255.0");
  Ipv4InterfaceContainer meshTreeInterface = address.Assign(connectMeshTree);

  // OSPF Routing
  OspfHelper ospfRoutingHelper;

  // Configure OSPF for the entire network
  ospfRoutingHelper.Install(ringNodes);
  ospfRoutingHelper.Install(busNodes);
  ospfRoutingHelper.Install(lineNodes);
  ospfRoutingHelper.Install(starNode);
  ospfRoutingHelper.Install(starLeafNodes);
  ospfRoutingHelper.Install(meshNodes);
  ospfRoutingHelper.Install(treeRoot);
  ospfRoutingHelper.Install(treeLeaves);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Application (UDP Echo)
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ringNodes.Get(3));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(ringInterfaces4.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(3));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable Tracing
  p2p.EnablePcapAll("hybrid_topology");

  // NetAnim Visualization
  AnimationInterface anim("hybrid_topology.xml");

  // Set node positions for better visualization
  anim.SetNodePosition(ringNodes.Get(0), 0, 10);
  anim.SetNodePosition(ringNodes.Get(1), 5, 10);
  anim.SetNodePosition(ringNodes.Get(2), 10, 10);
  anim.SetNodePosition(ringNodes.Get(3), 15, 10);

  anim.SetNodePosition(busNodes.Get(0), 0, 0);
  anim.SetNodePosition(busNodes.Get(1), 5, 0);
  anim.SetNodePosition(busNodes.Get(2), 10, 0);

  anim.SetNodePosition(lineNodes.Get(0), 20, 0);
  anim.SetNodePosition(lineNodes.Get(1), 25, 0);
  anim.SetNodePosition(lineNodes.Get(2), 30, 0);

  anim.SetNodePosition(starNode.Get(0), 25, 10);
  anim.SetNodePosition(starLeafNodes.Get(0), 20, 15);
  anim.SetNodePosition(starLeafNodes.Get(1), 25, 15);
  anim.SetNodePosition(starLeafNodes.Get(2), 30, 15);

  anim.SetNodePosition(meshNodes.Get(0), 0, 20);
  anim.SetNodePosition(meshNodes.Get(1), 5, 20);
  anim.SetNodePosition(meshNodes.Get(2), 0, 25);
  anim.SetNodePosition(meshNodes.Get(3), 5, 25);

  anim.SetNodePosition(treeRoot.Get(0), 25, 20);
  anim.SetNodePosition(treeLeaves.Get(0), 20, 25);
  anim.SetNodePosition(treeLeaves.Get(1), 30, 25);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}