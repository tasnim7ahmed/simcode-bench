#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/topology-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Disable logging for simplicity
    LogComponentDisableAll(LOG_LEVEL_ALL);

    // Create a star topology with 5 peripheral nodes and 1 central hub node
    uint32_t numPeripherals = 5;
    NodeContainer hub;
    hub.Create(1);

    NodeContainer peripherals;
    peripherals.Create(numPeripherals);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Connect each peripheral node to the hub
    for (uint32_t i = 0; i < numPeripherals; ++i) {
        NodeContainer pair(hub.Get(0), peripherals.Get(i));
        p2p.Install(pair);
    }

    // Assign IP addresses using Ipv4AddressHelper
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    // Assign addresses to all point-to-point links
    for (uint32_t i = 0; i < numPeripherals; ++i) {
        NetDeviceContainer devices = hub.Get(0)->GetDevice(i)->GetObject<PointToPointNetDevice>()->GetNode()->GetDeviceList()[0].GetObject<PointToPointNetDevice>()->GetChannel()->GetDevices();
        Ipv4InterfaceContainer interface = address.Assign(devices);
        address.NewNetwork();
    }

    // Print confirmation of connections
    std::cout << "Star topology established:" << std::endl;
    std::cout << "Central hub node ID: " << hub.Get(0)->GetId() << std::endl;
    std::cout << "Number of peripheral nodes: " << numPeripherals << std::endl;
    for (uint32_t i = 0; i < numPeripherals; ++i) {
        std::cout << "Peripheral node ID " << peripherals.Get(i)->GetId()
                  << " connected to hub via Point-to-Point link." << std::endl;
    }

    return 0;
}