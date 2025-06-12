#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;

    NodeContainer nodes;
    nodes.Create(numNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;
    Ipv4AddressHelper address;

    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer pair;
        pair.Add(nodes.Get(i));
        pair.Add(nodes.Get(i + 1));

        NetDeviceContainer devices = p2p.Install(pair);
        deviceContainers.push_back(devices);

        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        interfaceContainers.push_back(interfaces);
    }

    for (uint32_t i = 0; i < interfaceContainers.size(); ++i)
    {
        std::cout << "Link " << i+1 << " Node " << i
                  << " IP: " << interfaceContainers[i].GetAddress(0)
                  << " -- Node " << i+1
                  << " IP: " << interfaceContainers[i].GetAddress(1)
                  << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}