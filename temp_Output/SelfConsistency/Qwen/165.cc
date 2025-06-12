#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ospfv2-routing-protocol.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSquareTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("OspfSquareTopology", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between adjacent nodes forming a square
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1)); // A <-> B
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2)); // B <-> C
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3)); // C <-> D
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0)); // D <-> A

    // Install Internet stack with OSPF routing
    InternetStackHelper stack;
    Ipv4ListRoutingHelper listRH;
    Ospfv2Helper ospfRH;

    // Set OSPF areas (if needed, customize here)
    ospfRH.SetArea(0, 0); // Area 0 for all interfaces

    listRH.Add(ospfRH, 10); // priority 10 for OSPF
    stack.SetRoutingHelper(listRH);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer interfaces[4];

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    interfaces[0] = ipv4.Assign(devices[0]);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    interfaces[1] = ipv4.Assign(devices[1]);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    interfaces[2] = ipv4.Assign(devices[2]);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    interfaces[3] = ipv4.Assign(devices[3]);

    // Enable global routing to collect all static routes (OSPF handles dynamic)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Schedule routing table dumps for verification
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("ospf-square.routes");

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4Node = node->GetObject<Ipv4>();
        uint32_t nRoutes = ipv4Node->GetRoutingTableEntryCount();
        *stream->GetStream() << "Node " << node->GetId() << " Routing Table:" << std::endl;

        for (uint32_t j = 0; j < nRoutes; ++j) {
            Ipv4RoutingTableEntry route = ipv4Node->GetRoutingTableEntry(j);
            *stream->GetStream() << "  " << route.GetDestination() << "/" << route.GetPrefixToUse() << " -> "
                                 << route.GetNextHop() << " (interface: " << route.GetInterface() << ")" << std::endl;
        }
        *stream->GetStream() << std::endl;
    }

    Simulator::Schedule(Seconds(10.0), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(10.0), stream);

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}