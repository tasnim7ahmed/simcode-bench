#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numPeripherals = 5;

    CommandLine cmd;
    cmd.AddValue("numPeripherals", "Number of peripheral nodes in the star topology.", numPeripherals);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer hub;
    hub.Create(1);

    NodeContainer peripherals;
    peripherals.Create(numPeripherals);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    InternetStackHelper stack;
    stack.Install(hub);
    stack.Install(peripherals);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    for (uint32_t i = 0; i < numPeripherals; ++i) {
        NetDeviceContainer devices = p2p.Install(hub.Get(0), peripherals.Get(i));
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }

    NS_LOG_INFO("Star topology established with " << numPeripherals << " peripheral nodes connected to the central hub.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}