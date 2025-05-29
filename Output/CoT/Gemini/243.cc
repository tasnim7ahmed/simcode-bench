#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}