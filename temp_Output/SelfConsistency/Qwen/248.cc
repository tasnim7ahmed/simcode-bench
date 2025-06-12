#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numPeripherals = 5;

    CommandLine cmd;
    cmd.AddValue("numPeripherals", "Number of peripheral nodes", numPeripherals);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer hub;
    hub.Create(1);

    NodeContainer peripherals;
    peripherals.Create(numPeripherals);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;

    NS_LOG_INFO("Creating star topology with " << numPeripherals << " peripheral nodes.");

    for (uint32_t i = 0; i < numPeripherals; ++i) {
        NetDeviceContainer link = pointToPoint.Install(hub.Get(0), peripherals.Get(i));
        devices.Add(link);
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");

    for (uint32_t i = 0; i < numPeripherals; ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(devices.Get(2 * i), devices.Get(2 * i + 1)));
        NS_LOG_INFO("Interfaces between hub and peripheral " << i << ": "
                      << interfaces.GetAddress(0) << " <---> "
                      << interfaces.GetAddress(1));
    }

    NS_LOG_INFO("Star topology setup complete.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}