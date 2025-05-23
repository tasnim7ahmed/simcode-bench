#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create multiple nodes
    uint32_t numNodes = 5; // Example: 5 nodes for a mesh topology
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Point-to-Point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes in a mesh topology
    // Iterate through all unique pairs of nodes to create Point-to-Point links
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        for (uint32_t j = i + 1; j < numNodes; ++j) // j starts from i+1 to avoid duplicate pairs and self-loops
        {
            Ptr<Node> node1 = nodes.Get(i);
            Ptr<Node> node2 = nodes.Get(j);

            // Install devices on the pair of nodes
            NetDeviceContainer devices = pointToPoint.Install(node1, node2);

            // Print confirmation of the established connection
            std::cout << "Established Point-to-Point connection between Node " << node1->GetId()
                      << " and Node " << node2->GetId() << std::endl;
        }
    }

    // No simulation time is needed for just setting up the topology and printing connections
    // Simulator::Run();
    // Simulator::Destroy();

    return 0;
}