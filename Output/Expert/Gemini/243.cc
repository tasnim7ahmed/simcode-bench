#include "ns3/core-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    node->SetId(i);
  }

  std::cout << "Number of nodes created: " << NodeList::GetNNodes() << std::endl;

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}