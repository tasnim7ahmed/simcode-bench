#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  uint32_t numNodes = 5;
  cmd.AddValue ("numNodes", "Number of nodes to create", numNodes);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      std::cout << "Node " << i << " has NodeId: " << node->GetId () << std::endl;
    }

  return 0;
}