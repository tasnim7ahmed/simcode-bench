#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;
using namespace std;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4; // You can change the number of mesh nodes here

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install Internet stack (optional for address assignment, not strictly required)
    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    uint32_t connectionCount = 0;

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = i + 1; j < numNodes; ++j) {
            NodeContainer pair(nodes.Get(i), nodes.Get(j));
            NetDeviceContainer devices = p2p.Install(pair);

            // Optional: assign IP addresses to each link (not required for confirmation printout)
            /*
            std::ostringstream subnet;
            subnet << "10." << i << "." << j << ".0";
            Ipv4AddressHelper address;
            address.SetBase(subnet.str().c_str(), "255.255.255.0");
            address.Assign(devices);
            */

            std::cout << "Established point-to-point link between Node " << i
                      << " and Node " << j << std::endl;
            ++connectionCount;
        }
    }

    std::cout << "Total links established: " << connectionCount << std::endl;

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}