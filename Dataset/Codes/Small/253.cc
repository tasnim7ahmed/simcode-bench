#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("BusTopologyExample", LOG_LEVEL_INFO);

    // Define the number of nodes in the bus topology
    uint32_t numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // CSMA Helper (Shared Medium)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5)));

    // Install CSMA on all nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Log established connections
    for (uint32_t i = 0; i < numNodes; ++i) {
        NS_LOG_INFO("Node " << i << " connected to the shared CSMA channel.");
    }

    return 0;
}

