#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4; // Number of nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> netDevices;

    // Connect each pair of adjacent nodes in a simple line topology
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NetDeviceContainer link = p2p.Install(nodes.Get(i), nodes.Get(i+1));
        netDevices.push_back(link);
        std::cout << "Established P2P link between Node " << nodes.Get(i)->GetId()
                  << " and Node " << nodes.Get(i+1)->GetId() << std::endl;
    }

    // Optionally assign IPs (not strictly required per description)
    InternetStackHelper stack;
    stack.Install(nodes);

    for (uint32_t i = 0; i < netDevices.size(); ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        Ipv4AddressHelper address;
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(netDevices[i]);
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}