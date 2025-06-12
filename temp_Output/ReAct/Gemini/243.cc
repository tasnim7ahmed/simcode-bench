#include "ns3/core-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  std::cout << "Number of nodes: " << nodes.GetN() << std::endl;

  return 0;
}