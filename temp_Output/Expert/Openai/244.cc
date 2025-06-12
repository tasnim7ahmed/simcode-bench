#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 5;
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes to create", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        std::cout << "Node ID: " << node->GetId() << std::endl;
    }

    Simulator::Destroy();
    return 0;
}