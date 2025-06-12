#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipStarTopology");

int main(int argc, char *argv[]) {
    bool verbose = true;
    bool printRoutingTables = true;
    uint32_t simulationTime = 40;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Print trace information if true", verbose);
    cmd.AddValue("printRoutingTable", "Print routing tables at the end of the simulation", printRoutingTables);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RipStarTopology", LOG_LEVEL_INFO);
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
    }

    // Create nodes for a star topology: central router + 4 edge routers
    NodeContainer routers;
    routers.Create(5); // node 0 is central, nodes 1-4 are edge

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer routerDevices[5]; // Central connected to each edge

    for (uint32_t i = 1; i < 5; ++i) {
        routerDevices[i] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(i)));
    }

    InternetStackHelper stack;
    RipHelper rip;

    // Set up internet stack with RIP on all routers
    stack.SetRoutingHelper(rip);
    stack.Install(routers);

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer routerInterfaces[5];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.0." << i << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        routerInterfaces[i] = ipv4.Assign(routerDevices[i + 1]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Schedule(Seconds(20), &Ipv4GlobalRoutingHelper::PopulateRoutingTables);

    if (printRoutingTables) {
        Ptr<OutputStreamWrapper> routingStream = CreateFileStreamWrapper("rip-routing-tables.txt", std::ios::out);
        rip.PrintRoutingTableAllAt(Seconds(simulationTime - 1), routingStream);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}