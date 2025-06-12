#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FullMeshTopology");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes to create in the mesh.", numNodes);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");

    NS_LOG_INFO("Creating full mesh topology.");

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = i + 1; j < numNodes; ++j) {
            NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
            NetDeviceContainer devices = p2p.Install(pair);
            address.NewNetwork();
            Ipv4InterfaceContainer interfaces = address.Assign(devices);
            NS_LOG_INFO("Connected node " << i << " <-> " << j);
            std::cout << "Connection established between node " << i << " and node " << j << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}