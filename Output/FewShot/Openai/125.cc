#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/rip-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include <iostream>
#include <fstream>

using namespace ns3;

void PrintRoutingTable(Ptr<Node> node, Time printTime) {
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> prot = ipv4->GetRoutingProtocol();
    Ptr<Rip> rip = DynamicCast<Rip>(prot);
    if (!rip) return;
    std::ostringstream oss;
    oss << "Time: " << printTime.GetSeconds() << "s, Node: " << node->GetId() << "\n";
    rip->PrintRoutingTable(oss, Simulator::Now());
    std::cout << oss.str();
}

void ScheduleRoutingTablePrint(NodeContainer nodes, Time interval, Time stopTime) {
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Simulator::Schedule(interval, &PrintRoutingTable, nodes.Get(i), Simulator::Now() + interval);
    }
    if (Simulator::Now() + interval < stopTime) {
        Simulator::Schedule(interval, &ScheduleRoutingTablePrint, nodes, interval, stopTime);
    }
}

int main(int argc, char *argv[]) {
    // Log RIP routing
    LogComponentEnable("Rip", LOG_LEVEL_INFO);

    // Create a central node and four edge nodes
    NodeContainer central;
    central.Create(1);

    NodeContainer edges;
    edges.Create(4);

    NodeContainer allNodes;
    allNodes.Add(central);
    allNodes.Add(edges);

    // Configure Point-to-Point links and assign IPs
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    Ipv4InterfaceContainer interfaces[4];

    InternetStackHelper stack;
    RipHelper ripRouting;
    stack.SetRoutingHelper(ripRouting);
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    char baseIP[16];

    for (uint32_t i = 0; i < 4; ++i) {
        // Connect edge[i] <--> central[0]
        NodeContainer link;
        link.Add(edges.Get(i));
        link.Add(central.Get(0));
        devices[i] = p2p.Install(link);

        // Assign a /30 subnet for each link
        snprintf(baseIP, sizeof(baseIP), "10.1.%u.0", i+1);
        address.SetBase(baseIP, "255.255.255.252");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Enable global routing (not needed with RIP, but good practice)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Schedule route table printing every 5s
    ScheduleRoutingTablePrint(allNodes, Seconds(5.0), Seconds(40.0));

    // Run the simulation
    Simulator::Stop(Seconds(40.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}