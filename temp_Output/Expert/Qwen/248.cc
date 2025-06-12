#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopology");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5; // total nodes including hub

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the star topology (including hub)", numNodes);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("StarTopology", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Central node as hub
    Ptr<Node> hub = nodes.Get(0);
    PointToPointHelper p2p;

    // Set default attributes for point-to-point links
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 1; i < numNodes; ++i) {
        NodeContainer nlinks(hub, nodes.Get(i));
        NetDeviceContainer devs = p2p.Install(nlinks);
        Ipv4InterfaceContainer interfaces = address.Assign(devs);
        address.NewNetwork();
    }

    NS_LOG_INFO("Star topology built with " << numNodes << " nodes connected to a central hub.");
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}