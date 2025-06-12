#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nNodes = 5;

    CommandLine cmd;
    cmd.AddValue ("nNodes", "Number of nodes on the CSMA bus", nNodes);
    cmd.Parse (argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<NetDevice> dev = devices.Get(i);
        Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(dev);
        Ptr<Channel> chan = csmaDev->GetChannel();
        if (chan)
        {
            std::cout << "Node " << i << " is connected to CSMA channel." << std::endl;
        }
        else
        {
            std::cout << "Node " << i << " is NOT connected to any channel." << std::endl;
        }
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}