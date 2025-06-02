#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("NodeConnectionExample", LOG_LEVEL_INFO);

    // Create a container for two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Establish a Point-to-Point link between the nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Print confirmation of connection
    NS_LOG_INFO("Created a Point-to-Point link between Node " 
                << nodes.Get(0)->GetId() << " and Node " 
                << nodes.Get(1)->GetId());

    return 0;
}

