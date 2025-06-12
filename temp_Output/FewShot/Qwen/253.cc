#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes for the bus topology
    NodeContainer nodes;
    nodes.Create(4);  // Example with 4 nodes

    // Create a CSMA helper to set up the shared channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install devices on all nodes connected to the same shared channel
    NetDeviceContainer devices = csma.Install(nodes);

    // Print confirmation of connections
    std::cout << "Bus topology established with " << nodes.GetN() << " nodes connected to a shared CSMA channel." << std::endl;

    return 0;
}