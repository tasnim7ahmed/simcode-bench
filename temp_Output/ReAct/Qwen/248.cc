#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/ipv4-global-routing-helper.h"

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

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesHub;
    Ipv4InterfaceContainer interfacesHub;

    InternetStackHelper stack;
    stack.InstallAll();

    NS_LOG_INFO("Creating star topology.");
    for (uint32_t i = 0; i < numPeripherals; ++i) {
        NetDeviceContainer ndc = p2p.Install(NodeContainer(hub.Get(0), peripherals.Get(i)));
        Ipv4AddressHelper ipv4;
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer ic = ipv4.Assign(ndc);
        interfacesHub.Add(ic);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Verifying connections:");
    for (uint32_t i = 0; i < numPeripherals; ++i) {
        Ptr<Ipv4> ipv4_hub = hub.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4_peripheral = peripherals.Get(i)->GetObject<Ipv4>();

        Ipv4Address hubAddr = ipv4_hub->GetAddress(1 + i, 0).GetLocal();
        Ipv4Address periphAddr = ipv4_peripheral->GetAddress(1, 0).GetLocal();

        NS_LOG_INFO("Peripheral " << i << ": Hub IP: " << hubAddr << ", Peripheral IP: " << periphAddr);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}