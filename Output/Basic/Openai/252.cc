#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Create nodes: 1 root, 2 intermediate, 4 leaves (binary tree, depth 2)
    // 0: root, 1/2: intermediate, 3-6: leaves
    NodeContainer nodes;
    nodes.Create(7);

    // Helper for Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Store net devices (not used, but for completeness)
    std::vector<NetDeviceContainer> netDevices;

    // Connect root to intermediates (0-1, 0-2)
    NodeContainer nc_0_1(nodes.Get(0), nodes.Get(1));
    netDevices.push_back(p2p.Install(nc_0_1));
    NodeContainer nc_0_2(nodes.Get(0), nodes.Get(2));
    netDevices.push_back(p2p.Install(nc_0_2));

    // Intermediate 1 to leaves (1-3, 1-4)
    NodeContainer nc_1_3(nodes.Get(1), nodes.Get(3));
    netDevices.push_back(p2p.Install(nc_1_3));
    NodeContainer nc_1_4(nodes.Get(1), nodes.Get(4));
    netDevices.push_back(p2p.Install(nc_1_4));

    // Intermediate 2 to leaves (2-5, 2-6)
    NodeContainer nc_2_5(nodes.Get(2), nodes.Get(5));
    netDevices.push_back(p2p.Install(nc_2_5));
    NodeContainer nc_2_6(nodes.Get(2), nodes.Get(6));
    netDevices.push_back(p2p.Install(nc_2_6));

    // Print established connections
    std::cout << "Tree Topology Connections:" << std::endl;
    std::cout << "Root (Node 0) connected to Intermediate (Node 1)" << std::endl;
    std::cout << "Root (Node 0) connected to Intermediate (Node 2)" << std::endl;
    std::cout << "Intermediate (Node 1) connected to Leaf (Node 3)" << std::endl;
    std::cout << "Intermediate (Node 1) connected to Leaf (Node 4)" << std::endl;
    std::cout << "Intermediate (Node 2) connected to Leaf (Node 5)" << std::endl;
    std::cout << "Intermediate (Node 2) connected to Leaf (Node 6)" << std::endl;

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}