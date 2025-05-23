#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinearTopologyExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("LinearTopologyExample", LOG_LEVEL_INFO);

    // Number of nodes to create
    uint32_t numNodes = 5; 

    // Create the nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install the Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Configure Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Configure IP address assignment
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    // Connect nodes sequentially to form a linear (chain) topology
    // This loop creates (numNodes - 1) links
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        // Get two consecutive nodes to form a link
        Ptr<Node> node1 = nodes.Get(i);
        Ptr<Node> node2 = nodes.Get(i + 1);

        // Install Point-to-Point devices on these nodes
        NetDeviceContainer devices = p2p.Install(node1, node2);

        // Assign IP addresses to the installed devices
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Print confirmation of the established connection
        NS_LOG_INFO("Link " << i << ": Node " << node1->GetId() << " (IP: "
                             << interfaces.GetAddress(0) << ") <-> Node "
                             << node2->GetId() << " (IP: "
                             << interfaces.GetAddress(1) << ") established.");

        // Move to the next subnet for the next link
        ipv4.NewNetwork();
    }

    // Since this example only focuses on topology setup and confirmation,
    // we don't need a long simulation time or applications.
    Simulator::Stop(Seconds(0.1));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}