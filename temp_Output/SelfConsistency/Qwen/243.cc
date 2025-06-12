#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeCreationExample");

int
main(int argc, char* argv[])
{
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Verify node count
    NS_LOG_UNCOND("Number of created nodes: " << nodes.GetN());

    // Ensure the simulation runs long enough to see the log output
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}