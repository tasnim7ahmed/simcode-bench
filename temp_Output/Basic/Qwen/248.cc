#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numPeripherals = 5;

    NodeContainer hub;
    hub.Create(1);

    NodeContainer peripherals;
    peripherals.Create(numPeripherals);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesHub;
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");

    for (uint32_t i = 0; i < numPeripherals; ++i) {
        NetDeviceContainer link = p2p.Install(hub.Get(0), peripherals.Get(i));
        address.Assign(link);
        address.NewNetwork();
        NS_LOG_UNCOND("Connected peripheral node " << i << " to the hub.");
    }

    NS_LOG_UNCOND("Star topology setup complete with " << numPeripherals << " peripheral nodes connected to the central hub.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}