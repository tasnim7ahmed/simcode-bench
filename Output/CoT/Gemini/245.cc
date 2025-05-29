#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  Names::Add("Node0", nodes.Get(0));
  Names::Add("Node1", nodes.Get(1));
  Names::Add("Node2", nodes.Get(2));
  Names::Add("Node3", nodes.Get(3));
  Names::Add("Node4", nodes.Get(4));

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    std::cout << "Node ID: " << node->GetId() << ", Name: " << Names::FindName(node) << std::endl;
  }

  return 0;
}