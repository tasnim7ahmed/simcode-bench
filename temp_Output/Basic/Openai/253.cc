#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nNodes = 5;
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes in the CSMA bus", nNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    std::cout << "CSMA Bus Topology:" << std::endl;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<NetDevice> dev = devices.Get(i);
        Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(dev);
        if (csmaDev)
        {
            std::cout << "Node " << node->GetId() << " connected to CSMA channel." << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}