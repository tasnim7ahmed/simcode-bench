#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nodeCount = 4;

    CommandLine cmd;
    cmd.AddValue("nodeCount", "Number of nodes in the topology", nodeCount);
    cmd.Parse(argc, argv);

    if (nodeCount < 2)
    {
        NS_LOG_UNCOND("At least 2 nodes are required.");
        return 1;
    }

    NodeContainer nodes;
    nodes.Create(nodeCount);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;

    for (uint32_t i = 0; i < nodeCount - 1; ++i)
    {
        NodeContainer pair(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(pair);

        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        deviceContainers.push_back(devices);
        interfaceContainers.push_back(interfaces);

        NS_LOG_UNCOND("Established P2P link between Node " << nodes.Get(i)->GetId()
                                                           << " and Node " << nodes.Get(i + 1)->GetId());
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}