#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(i+1));
        deviceContainers.push_back(devices);
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        interfaceContainers.push_back(interfaces);
        address.NewNetwork();
    }

    Ptr<Ipv4> ipv4;
    std::cout << "Assigned IP addresses:" << std::endl;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ipv4Address addr = ipv4->GetAddress(1, 0).GetLocal();
        std::cout << "Node " << i << ": " << addr << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}