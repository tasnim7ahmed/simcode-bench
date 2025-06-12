#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set the number of nodes
    uint32_t numNodes = 4;
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes to create", numNodes);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install the Internet stack on each node
    InternetStackHelper stack;
    stack.Install(nodes);

    // Point-to-point helper for simplicity (creates interfaces)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create point-to-point links between consecutive nodes
    std::vector<NetDeviceContainer> devicesVec;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
        devicesVec.push_back(devices);
    }

    // Assign unique IP addresses to each point-to-point link
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfacesVec;
    for (uint32_t i = 0; i < devicesVec.size(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devicesVec[i]);
        interfacesVec.push_back(interfaces);
    }

    // Print assigned IP addresses for every node
    Ptr<Ipv4> ipv4;
    std::cout << "Assigned IP Addresses:\n";
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        for (uint32_t j = 1; j < ipv4->GetNInterfaces(); ++j) // interface 0 is loopback
        {
            Ipv4Address addr = ipv4->GetAddress(j, 0).GetLocal();
            std::cout << "Node " << i << " - Interface " << j << ": " << addr << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}