#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Assign unique node IDs for reference
    Ptr<Node> node0 = nodes.Get(0);
    Ptr<Node> node1 = nodes.Get(1);
    Ptr<Node> node2 = nodes.Get(2);

    // Configure a Point-to-Point link between Node 0 and Node 1
    PointToPointHelper p2p01;
    p2p01.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p01.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = p2p01.Install(node0, node1);

    // Configure another Point-to-Point link between Node 1 and Node 2
    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p12.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices12 = p2p12.Install(node1, node2);

    // Print confirmation of links established
    std::cout << "Link established between Node 0 and Node 1" << std::endl;
    std::cout << "Link established between Node 1 and Node 2" << std::endl;

    // Simulation initialization (no applications or traffic)
    Simulator::Initialize();

    // Run simulation for 0 seconds to initialize everything
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}