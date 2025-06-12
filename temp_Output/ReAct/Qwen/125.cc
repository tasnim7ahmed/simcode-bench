#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipStarTopology");

int main(int argc, char *argv[]) {
    bool verbose = true;
    uint32_t simulationTime = 30;

    if (verbose) {
        LogComponentEnable("RipStarTopology", LOG_LEVEL_INFO);
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL);
    }

    // Create nodes
    NodeContainer routers;
    routers.Create(5);  // One central router + four edge routers

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer routerDevices[5];

    // Connect each edge node to the central node (node 0)
    for (uint32_t i = 1; i < 5; ++i) {
        routerDevices[i] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(i)));
    }

    // Install Internet stack with RIP routing
    InternetStackHelper internet;
    RipHelper ripRouting;

    // Enable RIP on all nodes
    internet.SetRoutingHelper(ripRouting);
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer interfaces[5];
    std::ostringstream subnet;
    for (uint32_t i = 1; i < 5; ++i) {
        subnet << "10.0." << i << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = ipv4.Assign(routerDevices[i]);
        subnet.str("");
    }

    // Print routing tables at regular intervals
    Ptr<OutputStreamWrapper> routingStream = CreateFileStreamWrapper("rip-routing-tables.txt", std::ios::out);
    ripRouting.PrintRoutingTableAt(Seconds(10), routers, routingStream);
    ripRouting.PrintRoutingTableAt(Seconds(20), routers, routingStream);

    // Set simulation stop time
    Simulator::Stop(Seconds(simulationTime));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}