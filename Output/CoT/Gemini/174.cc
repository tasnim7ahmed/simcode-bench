#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopology");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("HybridTopology", LOG_LEVEL_INFO);

  // Create Nodes
  NodeContainer ringNodes;
  ringNodes.Create(5);

  NodeContainer meshNodes;
  meshNodes.Create(4);

  NodeContainer busNodes;
  busNodes.Create(3);

  NodeContainer lineNodes;
  lineNodes.Create(4);

  NodeContainer starNodes;
  starNodes.Create(3);

  NodeContainer treeNodes;
  treeNodes.Create(5);

  // Create PointToPoint Links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Ring Topology
  NetDeviceContainer ringDevices[5];
  for (int i = 0; i < 5; ++i) {
    ringDevices[i] = pointToPoint.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % 5));
  }

  // Mesh Topology
  NetDeviceContainer meshDevices[6];
  meshDevices[0] = pointToPoint.Install(meshNodes.Get(0), meshNodes.Get(1));
  meshDevices[1] = pointToPoint.Install(meshNodes.Get(0), meshNodes.Get(2));
  meshDevices[2] = pointToPoint.Install(meshNodes.Get(0), meshNodes.Get(3));
  meshDevices[3] = pointToPoint.Install(meshNodes.Get(1), meshNodes.Get(2));
  meshDevices[4] = pointToPoint.Install(meshNodes.Get(1), meshNodes.Get(3));
  meshDevices[5] = pointToPoint.Install(meshNodes.Get(2), meshNodes.Get(3));

  // Bus Topology
  NetDeviceContainer busDevices[2];
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  busDevices[0] = pointToPoint.Install(busNodes.Get(0), busNodes.Get(1));
  busDevices[1] = pointToPoint.Install(busNodes.Get(1), busNodes.Get(2));
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));

  // Line Topology
  NetDeviceContainer lineDevices[3];
  lineDevices[0] = pointToPoint.Install(lineNodes.Get(0), lineNodes.Get(1));
  lineDevices[1] = pointToPoint.Install(lineNodes.Get(1), lineNodes.Get(2));
  lineDevices[2] = pointToPoint.Install(lineNodes.Get(2), lineNodes.Get(3));

  // Star Topology
  NetDeviceContainer starDevices[2];
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  starDevices[0] = pointToPoint.Install(starNodes.Get(0), starNodes.Get(1));
  starDevices[1] = pointToPoint.Install(starNodes.Get(0), starNodes.Get(2));
      pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  
  // Tree Topology
  NetDeviceContainer treeDevices[4];
  treeDevices[0] = pointToPoint.Install(treeNodes.Get(0), treeNodes.Get(1));
  treeDevices[1] = pointToPoint.Install(treeNodes.Get(0), treeNodes.Get(2));
  treeDevices[2] = pointToPoint.Install(treeNodes.Get(1), treeNodes.Get(3));
  treeDevices[3] = pointToPoint.Install(treeNodes.Get(1), treeNodes.Get(4));
  

  // Connect the topologies
  NetDeviceContainer connection1 = pointToPoint.Install(ringNodes.Get(0), meshNodes.Get(0));
  NetDeviceContainer connection2 = pointToPoint.Install(busNodes.Get(0), lineNodes.Get(0));
  NetDeviceContainer connection3 = pointToPoint.Install(starNodes.Get(0), treeNodes.Get(0));
  NetDeviceContainer connection4 = pointToPoint.Install(ringNodes.Get(2), busNodes.Get(1));
  NetDeviceContainer connection5 = pointToPoint.Install(meshNodes.Get(2), starNodes.Get(1));
  NetDeviceContainer connection6 = pointToPoint.Install(lineNodes.Get(2), treeNodes.Get(2));

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(ringNodes);
  internet.Install(meshNodes);
  internet.Install(busNodes);
  internet.Install(lineNodes);
  internet.Install(starNodes);
  internet.Install(treeNodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces[5];
  for (int i = 0; i < 5; ++i) {
    ringInterfaces[i] = address.Assign(ringDevices[i]);
  }

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer meshInterfaces[6];
  for (int i = 0; i < 6; ++i) {
    meshInterfaces[i] = address.Assign(meshDevices[i]);
  }

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer busInterfaces[2];
  for (int i = 0; i < 2; ++i) {
    busInterfaces[i] = address.Assign(busDevices[i]);
  }

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer lineInterfaces[3];
  for (int i = 0; i < 3; ++i) {
    lineInterfaces[i] = address.Assign(lineDevices[i]);
  }

  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer starInterfaces[2];
  for (int i = 0; i < 2; ++i) {
    starInterfaces[i] = address.Assign(starDevices[i]);
  }

  address.SetBase("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer treeInterfaces[4];
  for (int i = 0; i < 4; ++i) {
    treeInterfaces[i] = address.Assign(treeDevices[i]);
  }

  address.SetBase("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer connectionInterfaces[6];
  connectionInterfaces[0] = address.Assign(connection1);
  connectionInterfaces[1] = address.Assign(connection2);
  connectionInterfaces[2] = address.Assign(connection3);
  connectionInterfaces[3] = address.Assign(connection4);
  connectionInterfaces[4] = address.Assign(connection5);
  connectionInterfaces[5] = address.Assign(connection6);

  // Install OSPF routing
  OlsrHelper olsr;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create Applications (Example: UDP Echo)
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ringNodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(ringInterfaces[4].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(3));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable Tracing
  pointToPoint.EnablePcapAll("hybrid");

  // Enable NetAnim
  AnimationInterface anim("hybrid.xml");

  // Run Simulation
  Simulator::Stop(Seconds(15.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}