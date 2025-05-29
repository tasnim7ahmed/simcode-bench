#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // Set up a simple tree: one root, two intermediates, each with two leaves
  //        root
  //        /  \
  //     int1  int2
  //     / \    / \
  //   l1  l2 l3  l4

  // Total nodes: 7
  NodeContainer nodes;
  nodes.Create (7);

  // Indexes:
  // 0: root
  // 1: int1
  // 2: int2
  // 3: l1
  // 4: l2
  // 5: l3
  // 6: l4

  // Link definitions (parent, child)
  std::vector<std::pair<uint32_t, uint32_t>> treeLinks = {
    {0, 1}, // root-int1
    {0, 2}, // root-int2
    {1, 3}, // int1-l1
    {1, 4}, // int1-l2
    {2, 5}, // int2-l3
    {2, 6}  // int2-l4
  };

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  std::vector<NetDeviceContainer> deviceContainers;

  for (const auto& [parent, child] : treeLinks)
    {
      NodeContainer pair (nodes.Get (parent), nodes.Get (child));
      NetDeviceContainer devices = p2p.Install (pair);
      deviceContainers.push_back (devices);

      std::cout << "Established Point-to-Point link between Node " << parent
                << " and Node " << child << std::endl;
    }

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}