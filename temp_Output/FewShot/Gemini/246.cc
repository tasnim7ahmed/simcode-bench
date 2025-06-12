#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure PointToPoint link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install NetDevices
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Print confirmation of links established
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<NetDevice> device = devices.Get(i);
        Ptr<Node> node = device->GetNode();
        std::cout << "Node " << node->GetId() << " has NetDevice" << std::endl;
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}