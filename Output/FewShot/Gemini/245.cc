#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Assign names to the nodes using the Object Name Service
    Names::Add(nodes.Get(0), "FirstNode");
    Names::Add(nodes.Get(1), "SecondNode");
    Names::Add(nodes.Get(2), "ThirdNode");
    Names::Add(nodes.Get(3), "FourthNode");
    Names::Add(nodes.Get(4), "FifthNode");

    // Print the names and IDs of the nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::cout << "Node " << i << " Name: " << Names::FindName(node)
                  << ", Node ID: " << node->GetId() << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}