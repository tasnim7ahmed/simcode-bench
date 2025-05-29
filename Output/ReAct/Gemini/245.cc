#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  Names::Add(nodes.Get(0), "NodeZero");
  Names::Add(nodes.Get(1), "NodeOne");
  Names::Add(nodes.Get(2), "NodeTwo");
  Names::Add(nodes.Get(3), "NodeThree");
  Names::Add(nodes.Get(4), "NodeFour");

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    std::cout << "Node ID: " << node->GetId() << ", Name: " << Names::FindName(node) << std::endl;
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}