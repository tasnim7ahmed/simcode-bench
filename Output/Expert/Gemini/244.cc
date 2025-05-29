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
    std::cout << "Node " << i << " ID: " << node->GetId() << std::endl;
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}