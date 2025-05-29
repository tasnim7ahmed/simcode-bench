#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinitySimulation");

// Helper function to print routing tables
void PrintRoutingTable (Ptr<Node> node, Time printTime)
{
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4RoutingTableEntry route;

    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
    Ptr<Rip> rip = DynamicCast<Rip> (proto);

    if (rip)
    {
        std::cout << "Node [" << node->GetId () << "] (" << Simulator::Now ().GetSeconds () << "s) RIP Routing Table:" << std::endl;
        rip->PrintRoutingTable (std::cout, Time::GetResolution (), Now ());
        std::cout << std::endl;
    }
}

// Periodically print routing tables of all nodes
void
PeriodicPrintRoutingTables (NodeContainer nodes, Time interval)
{
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        PrintRoutingTable(nodes.Get(i), Simulator::Now());
    }
    Simulator::Schedule (interval, &PeriodicPrintRoutingTables, nodes, interval);
}

int
main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("CountToInfinitySimulation", LOG_LEVEL_INFO);

    // Create nodes: A(0), B(1), C(2)
    NodeContainer nodes;
    nodes.Create (3);

    // Create point-to-point links: AB and BC
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer ndAB = p2p.Install (nodes.Get(0), nodes.Get(1)); // A-B
    NetDeviceContainer ndBC = p2p.Install (nodes.Get(1), nodes.Get(2)); // B-C

    // Install the internet stack with RIP on all nodes
    RipHelper ripRouting;
    ripRouting.ExcludeInterface (nodes.Get(0), 1); // No C link
    ripRouting.ExcludeInterface (nodes.Get(2), 0); // No A link

    Ipv4ListRoutingHelper listRH;
    listRH.Add (ripRouting, 0);

    InternetStackHelper internet;
    internet.SetRoutingHelper (listRH);
    internet.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.12.0", "255.255.255.0"); // AB
    Ipv4InterfaceContainer iAB = ipv4.Assign (ndAB);

    ipv4.SetBase ("10.0.23.0", "255.255.255.0"); // BC
    Ipv4InterfaceContainer iBC = ipv4.Assign (ndBC);

    // Set RIP infinity metric to 16 to highlight count-to-infinity
    ripRouting.SetInterfaceMetric (nodes.Get(0), 1, 1); // AB interface on A
    ripRouting.SetInterfaceMetric (nodes.Get(1), 0, 1); // AB interface on B
    ripRouting.SetInterfaceMetric (nodes.Get(1), 1, 1); // BC interface on B
    ripRouting.SetInterfaceMetric (nodes.Get(2), 0, 1); // BC interface on C

    // Enable global routing for completeness
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Schedule periodic routing table printing every 2 seconds
    Simulator::Schedule (Seconds (2.0), &PeriodicPrintRoutingTables, nodes, Seconds (2.0));

    // Allow routers to exchange routes and converge for a short time
    Simulator::Stop (Seconds (5.0));
    Simulator::Run ();
    Simulator::Stop ();

    // Now break the link between B and C at 5s
    NS_LOG_INFO ("*** Breaking link B-C at 5s ***");
    Ptr<NetDevice> ndB = ndBC.Get(0);
    Ptr<NetDevice> ndC = ndBC.Get(1);
    ndB->SetMtu (0); // Hack: set MTU to 0 to simulate down
    ndC->SetMtu (0);

    // Alternatively, you can use Disable():
    // ndB->SetReceiveCallback (MakeNullCallback<bool,Ptr<NetDevice>,Ptr<const Packet>,uint16_t,const Address &>());
    // ndC->SetReceiveCallback (MakeNullCallback<bool,Ptr<NetDevice>,Ptr<const Packet>,uint16_t,const Address &>());

    // Resume simulation after link break
    Simulator::Stop (Seconds (40.0));
    Simulator::Run ();

    // Final routing tables
    std::cout << "*** Final Routing Tables ***" << std::endl;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        PrintRoutingTable(nodes.Get(i), Simulator::Now());
    }

    Simulator::Destroy ();
    return 0;
}