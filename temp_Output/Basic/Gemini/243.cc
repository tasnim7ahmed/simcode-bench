#include "ns3/core-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    std::cout << "Node " << i << " ID: " << node->GetId() << std::endl;
  }

  return 0;
}