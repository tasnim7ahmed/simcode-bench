#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BusTopologyExample");

int main(int argc, char *argv[]) {
    // Enable logging for this example
    LogComponentEnable("BusTopologyExample", LOG_LEVEL_INFO);

    // Create nodes for the bus topology
    uint32_t numNodes = 5;
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create a shared CSMA channel
    CsmaChannelHelper csmaChannelHelper;
    csmaChannelHelper.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csmaChannelHelper.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    // Create a device container to hold all the net devices
    NetDeviceContainer devices;

    // Connect each node sequentially to the shared channel (bus topology)
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<NetDevice> device = csmaChannelHelper.Install(node);
        devices.Add(device);
        NS_LOG_INFO("Node " << i << " connected to the bus.");
    }

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses (required even if not used in this example)
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("Bus topology setup completed successfully.");

    // Simulation run and destroy (no actual simulation time needed for setup confirmation)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}