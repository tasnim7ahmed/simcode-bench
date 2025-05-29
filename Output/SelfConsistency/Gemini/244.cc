#include <iostream>
#include <ns3/core-module.h>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t numNodes = 5; // Default number of nodes

  NodeContainer nodes;
  nodes.Create(numNodes);

  std::cout << "Node IDs:" << std::endl;
  for (uint32_t i = 0; i < numNodes; ++i) {
    Ptr<Node> node = nodes.Get(i);
    std::cout << "Node " << i << ": ID = " << node->GetId() << std::endl;
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}