#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nNodes = 5;

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes in the bus topology", nNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<NetDevice> dev = devices.Get(i);
        Ptr<Node> node = nodes.Get(i);
        std::cout << "Node " << node->GetId() << " connected with device " << dev->GetIfIndex() << std::endl;
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}