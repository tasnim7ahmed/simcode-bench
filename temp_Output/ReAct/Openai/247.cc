#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Link nodes in a line topology
    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<Ipv4InterfaceContainer> interfaces;

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Assign address base for subnets
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(i+1));
        deviceContainers.push_back(devices);
        Ipv4InterfaceContainer iface = address.Assign(devices);
        interfaces.push_back(iface);
        address.NewNetwork();
    }

    // Print the assigned IP addresses
    Ptr<Ipv4> ipv4;
    std::cout << "Assigned IP addresses:" << std::endl;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        std::cout << "Node " << i << ":" << std::endl;
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j)
        {
            for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k)
            {
                Ipv4InterfaceAddress addr = ipv4->GetAddress(j, k);
                if (addr.GetAddress() != Ipv4Address("127.0.0.1"))
                {
                    std::cout << "  Interface " << j
                              << ": " << addr.GetAddress() << std::endl;
                }
            }
        }
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}