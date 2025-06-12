#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Define the number of nodes in the bus topology
    uint32_t numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create a shared channel
    Ptr<Channel> channel = CreateObject<CsmaChannel>();

    // Connect each node to the shared channel using a CsmaDevice
    CsmaHelper csma;
    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<NetDevice> device = csma.Install(nodes.Get(i), channel);
        devices.Add(device);
    }

    // Print confirmation of connections
    std::cout << "Bus topology established with " << numNodes << " nodes connected to a shared CSMA channel." << std::endl;

    return 0;
}