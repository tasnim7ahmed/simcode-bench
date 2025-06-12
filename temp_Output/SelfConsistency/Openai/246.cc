#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set simulation parameters
    uint32_t numNodes = 4; // Number of nodes in the network

    // Enable logging
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Create Point-to-Point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links between consecutive nodes (linear topology)
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer pair(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(pair);

        NS_LOG_UNCOND("Established P2P link between Node " << pair.Get(0)->GetId()
                                                           << " and Node " << pair.Get(1)->GetId());
    }

    // Assign IP addresses (not strictly necessary for this example, but standard practice)
    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        NodeContainer pair(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = nodes.Get(i)->GetDevice(0)->GetChannel()->GetNDevices() == 2 ?
            NetDeviceContainer(pair.Get(0)->GetDevice(i), pair.Get(1)->GetDevice(0)) :
            NetDeviceContainer();

        // Instead, since the NetDeviceContainer isn't available, we can fetch the last two net devices
        NetDeviceContainer lastDevices;
        lastDevices.Add(nodes.Get(i)->GetDevice(nodes.Get(i)->GetNDevices() - 1));
        lastDevices.Add(nodes.Get(i + 1)->GetDevice(nodes.Get(i + 1)->GetNDevices() - 1));
        address.Assign(lastDevices);
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}