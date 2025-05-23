#include "ns3/core-module.h"
#include "ns3/node-container.h"
#include "ns3/names.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ns3NodeNamingExample");

int
main (int argc, char *argv[])
{
  // Enable logging for this component
  LogComponentEnable ("Ns3NodeNamingExample", LOG_LEVEL_INFO);

  // Create a container for nodes
  NodeContainer nodes;
  uint32_t numNodes = 5;
  nodes.Create (numNodes);

  NS_LOG_INFO ("Assigning names to nodes...");

  // Assign individual names to each node using Object Name Service
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      std::string nodeName = "MyNode-" + std::to_string (i);
      Names::Add (nodeName, node);
      NS_LOG_INFO ("Assigned name '" << nodeName << "' to Node ID " << node->GetId());
    }

  NS_LOG_INFO ("Printing assigned names and node IDs:");

  // Iterate through nodes and print their assigned names and unique IDs
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      std::string retrievedName = Names::FindNameByObject (node);
      std::cout << "Node ID: " << node->GetId ()
                << ", Assigned Name: " << retrievedName << std::endl;
    }

  // Note: No simulation is run since this example only deals with object creation and naming,
  // not network events.
  return 0;
}