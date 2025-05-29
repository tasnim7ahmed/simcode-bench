#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Simple tree topology:
  //         0
  //       /   \
  //      1     2
  //     / \   / \
  //    3  4  5   6

  // Total 7 nodes: 1 root, 2 intermediate, 4 leaves
  NodeContainer nodes;
  nodes.Create (7);

  // Install internet stack (optional, but common)
  InternetStackHelper stack;
  stack.Install (nodes);

  // Point-to-point helper
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Store NetDeviceContainers for possible later use
  std::vector<NetDeviceContainer> deviceContainers;

  // Root to intermediate nodes
  NetDeviceContainer nd0_1 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer nd0_2 = p2p.Install (nodes.Get (0), nodes.Get (2));
  deviceContainers.push_back (nd0_1);
  deviceContainers.push_back (nd0_2);

  // Intermediate 1 to its children (3,4)
  NetDeviceContainer nd1_3 = p2p.Install (nodes.Get (1), nodes.Get (3));
  NetDeviceContainer nd1_4 = p2p.Install (nodes.Get (1), nodes.Get (4));
  deviceContainers.push_back (nd1_3);
  deviceContainers.push_back (nd1_4);

  // Intermediate 2 to its children (5,6)
  NetDeviceContainer nd2_5 = p2p.Install (nodes.Get (2), nodes.Get (5));
  NetDeviceContainer nd2_6 = p2p.Install (nodes.Get (2), nodes.Get (6));
  deviceContainers.push_back (nd2_5);
  deviceContainers.push_back (nd2_6);

  // Print established connections
  std::cout << "Tree topology connections established:" << std::endl;
  std::cout << "Root node 0 connected to intermediate nodes 1 and 2" << std::endl;
  std::cout << "Intermediate node 1 connected to leaf nodes 3 and 4" << std::endl;
  std::cout << "Intermediate node 2 connected to leaf nodes 5 and 6" << std::endl;

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}