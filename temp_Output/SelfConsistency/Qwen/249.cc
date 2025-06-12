#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinearTopologyExample");

int
main(int argc, char *argv[])
{
    uint32_t numNodes = 5;

    // Enable logging
    LogComponentEnable("LinearTopologyExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    // Install devices and link them sequentially
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = pointToPoint.Install(pair);
        std::ostringstream oss;
        oss << "Link established between Node " << i << " and Node " << (i + 1);
        NS_LOG_INFO(oss.str());
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}