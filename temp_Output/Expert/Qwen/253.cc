#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    NodeContainer nodes;
    nodes.Create(numNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer link = csma.Install(pair);
        devices.Add(link);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(devices.Get(i));
    }

    std::cout << "Bus topology established with " << numNodes << " nodes connected via CSMA." << std::endl;

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}