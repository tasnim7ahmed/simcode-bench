#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-rip.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for RIP
    LogComponentEnable("Rip", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4Interface", LOG_LEVEL_INFO);

    // Create a star topology: 1 central node + 4 edge nodes
    NodeContainer routers;
    routers.Create(5);  // Node 0 is the central router

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    Ipv4InterfaceContainer interfaces[4];

    // Connect each edge node to the central node (node 0)
    PointToPointNetDeviceHelper p2pHelper;
    InternetStackHelper stack;
    Ipv4AddressHelper address;

    // Install internet stack on all nodes
    stack.Install(routers);

    // Enable RIP protocol on all routers
    RipHelper ripRouting;
    ripRouting.ExcludeInterface(routers.Get(0), 1); // Exclude loopback
    ripRouting.SetInterfaceMetric(routers.Get(0), 1, 1); // Set metric for interface 1

    Ipv4ListRoutingHelper listRH;
    listRH.Add(ripRouting, 0);

    InternetStackHelper stackWithRip;
    stackWithRip.SetRoutingHelper(listRH);
    stackWithRip.Install(routers);

    // Assign IP addresses
    char subnetBase[16];
    for (uint32_t i = 0; i < 4; ++i) {
        sprintf(subnetBase, "10.1.%d.0", i + 1);
        address.SetBase(subnetBase, "255.255.255.0");
        devices[i] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(i + 1)));
        interfaces[i] = address.Assign(devices[i]);
    }

    // Trace routing tables
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> routingStream = ascii.CreateFileStream("rip.routes");
    Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt(Seconds(2.0), routingStream);
    Ipv4GlobalRoutingHelper::PrintRoutingTableAllEvery(Seconds(5.0), routingStream);

    // Run simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}