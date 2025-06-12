#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  // Set up logging if required
  // LogComponentEnable("NodeIdExample", LOG_LEVEL_INFO);

  // Number of nodes to create
  uint32_t numNodes = 5;

  // Create the nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // Print Node IDs
  for (NodeContainer::Iterator it = nodes.Begin (); it != nodes.End (); ++it)
    {
      Ptr<Node> node = *it;
      uint32_t nodeId = node->GetId ();
      std::cout << "Node ID: " << nodeId << std::endl;
    }

  return 0;
}