#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nNodes = 5;

    NodeContainer nodes;
    nodes.Create(nNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    for (uint32_t i = 0; i < nNodes; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<NetDevice> dev = devices.Get(i);
        NS_LOG_UNCOND("Node " << node->GetId() << " connected to CSMA channel via device " << dev->GetIfIndex());
    }

    return 0;
}