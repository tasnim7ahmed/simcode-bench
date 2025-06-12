#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  Names::Add("node-0", nodes.Get(0));
  Names::Add("node-1", nodes.Get(1));
  Names::Add("node-2", nodes.Get(2));
  Names::Add("node-3", nodes.Get(3));
  Names::Add("node-4", nodes.Get(4));

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    std::cout << "Node " << node->GetId() << " name: " << Names::FindName(node) << std::endl;
  }

  Simulator::Destroy();
  return 0;
}