#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t numNodes = 5;

  NodeContainer nodes;
  nodes.Create(numNodes);

  for (uint32_t i = 0; i < numNodes; ++i) {
    Ptr<Node> node = nodes.Get(i);
    uint32_t nodeId = node->GetId();
    std::cout << "Node " << i << " has Node ID: " << nodeId << std::endl;
  }

  return 0;
}