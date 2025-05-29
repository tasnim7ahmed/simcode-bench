#include "ns3/core-module.h"
#include <iostream>

using namespace ns3;

int main() {
  // Enable logging (optional)
  // LogComponentEnable ("NodeExample", LOG_LEVEL_INFO);

  // Create three nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Assign unique IDs (optional, but good practice)
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    node->SetId(i);
  }

  // Print the number of nodes created
  std::cout << "Number of nodes created: " << NodeList::GetNNodes() << std::endl;

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}