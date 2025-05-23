#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ospf-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // 1. Create four nodes
    NodeContainer nodes;
    nodes.Create(4); // N0, N1, N2, N3

    // 2. Create PointToPoint links and install devices
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01, d12, d23, d30;

    d01 = p2p.Install(nodes.Get(0), nodes.Get(1)); // N0-N1
    d12 = p2p.Install(nodes.Get(1), nodes.Get(2)); // N1-N2
    d23 = p2p.Install(nodes.Get(2), nodes.Get(3)); // N2-N3
    d30 = p2p.Install(nodes.Get(3), nodes.Get(0)); // N3-N0

    // 3. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer i01, i12, i23, i30;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    i01 = ipv4.Assign(d01);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    i12 = ipv4.Assign(d12);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    i23 = ipv4.Assign(d23);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    i30 = ipv4.Assign(d30);

    // 4. Install OSPF routing on all nodes
    Ipv4OspfHelper ospf;
    Ipv4ListRoutingHelper listRouting;

    listRouting.Add(ospf, 0); // Add OSPF with priority 0

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting); // Set the routing helper to use OSPF
    internet.Install(nodes);

    // All interfaces should be added to OSPF Area 0 (backbone area)
    ospf.AddAllRoutersToArea(nodes, 0);

    // 5. Populate routing tables
    ospf.PopulateRoutingTables();

    // 6. Monitor routing tables
    // Print routing tables at 1 second and 10 seconds to observe convergence
    // The output files are written to /tmp/
    Simulator::Schedule(Seconds(1.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 1.0, nodes.Get(0)->GetId(), "/tmp/routing-table-N0-1s.txt");
    Simulator::Schedule(Seconds(1.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 1.0, nodes.Get(1)->GetId(), "/tmp/routing-table-N1-1s.txt");
    Simulator::Schedule(Seconds(1.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 1.0, nodes.Get(2)->GetId(), "/tmp/routing-table-N2-1s.txt");
    Simulator::Schedule(Seconds(1.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 1.0, nodes.Get(3)->GetId(), "/tmp/routing-table-N3-1s.txt");

    Simulator::Schedule(Seconds(10.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 10.0, nodes.Get(0)->GetId(), "/tmp/routing-table-N0-10s.txt");
    Simulator::Schedule(Seconds(10.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 10.0, nodes.Get(1)->GetId(), "/tmp/routing-table-N1-10s.txt");
    Simulator::Schedule(Seconds(10.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 10.0, nodes.Get(2)->GetId(), "/tmp/routing-table-N2-10s.txt");
    Simulator::Schedule(Seconds(10.0), &Ipv4RoutingHelper::PrintRoutingTableAt,
                        &listRouting, 10.0, nodes.Get(3)->GetId(), "/tmp/routing-table-N3-10s.txt");
    
    // Stop simulation after sufficient time for convergence and monitoring
    Simulator::Stop(Seconds(15.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}