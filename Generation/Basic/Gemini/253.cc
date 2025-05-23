#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // Enable logging for the CSMA component (optional, for debugging)
    // LogComponentEnable("CsmaHelper", LOG_LEVEL_INFO);
    // LogComponentEnable("CsmaChannel", LOG_LEVEL_INFO);
    // LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);

    // Create multiple nodes for the bus topology
    NodeContainer nodes;
    nodes.Create(5); // Creating 5 nodes

    // Create a CSMA helper and set channel attributes
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560))); // Typical Ethernet delay for 1km

    // Install CSMA devices on all nodes and connect them to a shared CSMA channel
    NetDeviceContainer devices = csma.Install(nodes);

    // Print confirmation of the established connections
    std::cout << "CSMA Bus Topology Setup Confirmation:" << std::endl;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<NetDevice> device = devices.Get(i);
        std::cout << "  Node " << node->GetId() << " has NetDevice " << device->GetIfIndex()
                  << " (Type: " << device->GetTypeId().GetName() << ")" << std::endl;
    }

    // Run the simulation (even if no events are explicitly scheduled, required for framework setup)
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}