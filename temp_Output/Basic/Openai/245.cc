#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create 5 nodes
  NodeContainer nodes;
  nodes.Create(5);

  // Assign individual names using Names::Add
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      std::ostringstream oss;
      oss << "Node-" << i;
      Names::Add(oss.str(), nodes.Get(i));
    }

  // Print names and IDs
  std::cout << "Assigned node names and IDs:" << std::endl;
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      std::string name = Names::FindName(node);
      uint32_t nodeId = node->GetId();
      std::cout << "Node ID: " << nodeId << ", Name: " << name << std::endl;
    }

  return 0;
}