#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create 5 nodes
  NodeContainer nodes;
  nodes.Create(5);

  // Assign names using Object Name Service (ONS)
  Names::Add("NodeA", nodes.Get(0));
  Names::Add("NodeB", nodes.Get(1));
  Names::Add("NodeC", nodes.Get(2));
  Names::Add("NodeD", nodes.Get(3));
  Names::Add("NodeE", nodes.Get(4));

  // Print the names and node IDs
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      std::string nodeName = Names::FindName(node);
      uint32_t nodeId = node->GetId();
      std::cout << "Node ID: " << nodeId << ", Name: " << nodeName << std::endl;
    }

  Simulator::Stop(Seconds(1.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}