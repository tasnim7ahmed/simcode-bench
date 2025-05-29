#include "ns3/core-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nNodes = 5;

  NodeContainer nodes;
  nodes.Create(nNodes);

  for (uint32_t i = 0; i < nNodes; ++i) {
    Ptr<Node> node = nodes.Get(i);
    std::cout << "Node " << i << " ID: " << node->GetId() << std::endl;
  }

  return 0;
}