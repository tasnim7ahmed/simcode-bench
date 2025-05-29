#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    std::cout << "Node " << i << " has system ID: " << node->GetId() << std::endl;
  }

  Simulator::Destroy();
  return 0;
}