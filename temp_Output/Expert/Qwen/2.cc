#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create two Node objects
    Ptr<Node> node0 = CreateObject<Node>();
    Ptr<Node> node1 = CreateObject<Node>();

    // Create a NodeContainer to hold the nodes
    NodeContainer nodes;
    nodes.Add(node0);
    nodes.Add(node1);

    // Create a PointToPointHelper to set up the link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the point-to-point devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Verify that two devices were successfully created and connected
    NS_ASSERT_MSG(devices.GetN() == 2, "Point-to-point link setup failed: expected 2 devices, got " << devices.GetN());

    // Run the simulation (no traffic is generated, so it exits immediately)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}