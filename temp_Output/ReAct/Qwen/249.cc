#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinearTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(numNodes);

    NS_LOG_INFO("Creating point-to-point links.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NetDeviceContainer linkDevices = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
        devices.Add(linkDevices);
    }

    NS_LOG_INFO("Assigning IP addresses.");
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(devices.Get(2 * i), devices.Get(2 * i + 1)));
        address.NewNetwork();
    }

    NS_LOG_INFO("Connections established successfully in linear topology.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}