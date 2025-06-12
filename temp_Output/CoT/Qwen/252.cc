#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExample");

int main(int argc, char *argv[]) {
  uint32_t depth = 2;
  uint32_t fanout = 2;

  CommandLine cmd(__FILE__);
  cmd.AddValue("depth", "Depth of the tree (excluding root)", depth);
  cmd.AddValue("fanout", "Fanout of each node in the tree", fanout);
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes;
  nodes.Create(1); // Root node

  std::vector<NodeContainer> nodeLevels;
  nodeLevels.push_back(nodes); // Level 0: root

  for (uint32_t level = 1; level <= depth; ++level) {
    NodeContainer currentLevel;
    currentLevel.Create(fanout);
    nodeLevels.push_back(currentLevel);
  }

  NS_LOG_INFO("Creating point-to-point links.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t level = 0; level < depth; ++level) {
    NetDeviceContainer parentDevices;
    for (uint32_t i = 0; i < nodeLevels[level].GetN(); ++i) {
      Node* parent = nodeLevels[level].Get(i);
      for (uint32_t j = 0; j < fanout; ++j) {
        Node* child = nodeLevels[level + 1].Get(i * fanout + j);
        NodeContainer pair(parent, child);
        NetDeviceContainer devices = p2p.Install(pair);
        parentDevices.Add(devices.Get(0));
      }
    }
  }

  NS_LOG_INFO("Tree topology created successfully with depth " << depth << " and fanout " << fanout);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}