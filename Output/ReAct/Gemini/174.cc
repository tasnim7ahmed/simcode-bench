#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/olsr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopology");

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("HybridTopology", LOG_LEVEL_INFO);

  // Create Nodes
  NodeContainer ringNodes, meshNodes, busNodes, lineNodes, starNodes, treeNodes;
  ringNodes.Create(4);
  meshNodes.Create(4);
  busNodes.Create(3);
  lineNodes.Create(2);
  starNodes.Create(5);
  treeNodes.Create(7);

  NodeContainer allNodes;
  allNodes.Add(ringNodes);
  allNodes.Add(meshNodes);
  allNodes.Add(busNodes);
  allNodes.Add(lineNodes);
  allNodes.Add(starNodes);
  allNodes.Add(treeNodes);


  // Create PointToPointHelper
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Build Ring Topology
  NetDeviceContainer ringDevices[4];
  for (int i = 0; i < 4; ++i) {
    ringDevices[i] = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % 4));
  }

  // Build Mesh Topology
  NetDeviceContainer meshDevices[6];
  meshDevices[0] = p2p.Install(meshNodes.Get(0), meshNodes.Get(1));
  meshDevices[1] = p2p.Install(meshNodes.Get(0), meshNodes.Get(2));
  meshDevices[2] = p2p.Install(meshNodes.Get(0), meshNodes.Get(3));
  meshDevices[3] = p2p.Install(meshNodes.Get(1), meshNodes.Get(2));
  meshDevices[4] = p2p.Install(meshNodes.Get(1), meshNodes.Get(3));
  meshDevices[5] = p2p.Install(meshNodes.Get(2), meshNodes.Get(3));


  // Build Bus Topology
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  NetDeviceContainer busDevices = csma.Install(busNodes);

  // Build Line Topology
  NetDeviceContainer lineDevices = p2p.Install(lineNodes.Get(0), lineNodes.Get(1));

  // Build Star Topology
  NetDeviceContainer starDevices[5];
  for (int i = 1; i < 5; ++i) {
    starDevices[i] = p2p.Install(starNodes.Get(0), starNodes.Get(i));
  }

    // Build Tree Topology - simple binary tree
    NetDeviceContainer treeDevices[6];
    treeDevices[0] = p2p.Install(treeNodes.Get(0), treeNodes.Get(1));
    treeDevices[1] = p2p.Install(treeNodes.Get(0), treeNodes.Get(2));
    treeDevices[2] = p2p.Install(treeNodes.Get(1), treeNodes.Get(3));
    treeDevices[3] = p2p.Install(treeNodes.Get(1), treeNodes.Get(4));
    treeDevices[4] = p2p.Install(treeNodes.Get(2), treeNodes.Get(5));
    treeDevices[5] = p2p.Install(treeNodes.Get(2), treeNodes.Get(6));

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(allNodes);

  // Assign IP Addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ringInterfaces[4];
    for (int i = 0; i < 4; ++i) {
        ringInterfaces[i] = address.Assign(ringDevices[i]);
        address.NewNetwork();
    }

    Ipv4InterfaceContainer meshInterfaces[6];
    for(int i = 0; i < 6; i++){
        meshInterfaces[i] = address.Assign(meshDevices[i]);
        address.NewNetwork();
    }

  Ipv4InterfaceContainer busInterfaces = address.Assign(busDevices);
  address.NewNetwork();

  Ipv4InterfaceContainer lineInterfaces = address.Assign(lineDevices);
  address.NewNetwork();

  Ipv4InterfaceContainer starInterfaces[5];
  for (int i = 1; i < 5; ++i) {
    starInterfaces[i] = address.Assign(starDevices[i]);
    address.NewNetwork();
  }

    Ipv4InterfaceContainer treeInterfaces[6];
    for(int i = 0; i < 6; i++){
        treeInterfaces[i] = address.Assign(treeDevices[i]);
        address.NewNetwork();
    }


  // Enable OSPF Routing
  OlsrHelper olsr;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create Applications (Example: UDP Echo Client/Server)
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ringNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(ringInterfaces[0].GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable Tracing
  p2p.EnablePcapAll("hybrid_topology");

  // Enable NetAnim
  AnimationInterface anim("hybrid_topology.xml");
  anim.EnablePacketMetadata(true);

  // Run Simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}