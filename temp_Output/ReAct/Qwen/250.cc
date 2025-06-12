#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;
    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the mesh.", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    std::vector<std::pair<Ipv4InterfaceContainer, Ipv4InterfaceContainer>> interfacePairs;

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = i + 1; j < numNodes; ++j) {
            NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(j));
            address.SetBase("10.1." + std::to_string(i) + "." + std::to_string(j), "255.255.255.0");
            Ipv4InterfaceContainer interfaces = address.Assign(devices);
            NS_LOG_INFO("Created link between Node " << i << " and Node " << j);
            std::cout << "Link established between Node " << i << " and Node " << j << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}