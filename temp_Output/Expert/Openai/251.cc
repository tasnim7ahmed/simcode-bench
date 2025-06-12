#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nNodes = 6;
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes in the ring", nNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devices;
    std::cout << "Establishing ring topology with " << nNodes << " nodes..." << std::endl;

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        uint32_t next = (i + 1) % nNodes;
        NetDeviceContainer link = p2p.Install(nodes.Get(i), nodes.Get(next));
        devices.push_back(link);
        std::cout << "Link established between Node[" << i << "] <--> Node[" << next << "]" << std::endl;
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}