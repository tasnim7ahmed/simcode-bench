#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BusTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    NodeContainer nodes;
    nodes.Create(numNodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100 Mbps)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        PointToPointHelper p2p;
        NetDeviceContainer link = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
        devices.Add(link);
    }

    CsmaHelper csmaShared;
    csmaShared.SetChannelAttribute("DataRate", DataRateValue(DataRate(100 Mbps)));
    csmaShared.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    Ptr<Node> switchNode = nodes.Get(0);
    for (uint32_t i = 1; i < numNodes; ++i) {
        NetDeviceContainer link = csmaShared.Install(NodeContainer(switchNode, nodes.Get(i)));
        devices.Add(link);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("Bus topology established with " << numNodes << " nodes connected to a shared CSMA channel.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}