#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

void PrintRoutingTable (Ptr<Node> node, Time printTime)
{
    Ipv4RoutingHelper *rh = nullptr;
    ObjectVectorValue list;
    node->GetObject<Ipv4>()->GetAttribute("RoutingProtocols", list);
    for (uint32_t i = 0; i < list.GetN (); ++i)
    {
        Ptr<Ipv4RoutingProtocol> proto = list.Get(i)->GetObject<Ipv4RoutingProtocol>();
        if (proto)
        {
            Ptr<OspfRoutingProtocol> ospf = DynamicCast<OspfRoutingProtocol> (proto);
            if (ospf)
            {
                std::ostringstream oss;
                oss << "Routing table of node " << node->GetId () << " at " << Simulator::Now ().GetSeconds () << "s";
                ospf->PrintRoutingTable (Create<OutputStreamWrapper>(&std::cout), Time::S (0), oss.str ());
                break;
            }
        }
    }
}

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create (4);

    // Square topology: N0---N1
    //                 |     |
    //                 N3---N2

    // Setup the links (point-to-point)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer d01 = p2p.Install (nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install (nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install (nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install (nodes.Get(3), nodes.Get(0));

    // Optionally, add the diagonal for more interesting shortest path, but keeping the square

    // Install Internet stack with OSPF as the routing protocol

    OspfRoutingHelper ospf;
    Ipv4ListRoutingHelper list;
    list.Add (ospf, 10); // OSPF priority
    InternetStackHelper stack;
    stack.SetRoutingHelper (list);
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    // N0-N1
    ipv4.SetBase ("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = ipv4.Assign (d01);

    // N1-N2
    ipv4.SetBase ("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = ipv4.Assign (d12);

    // N2-N3
    ipv4.SetBase ("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = ipv4.Assign (d23);

    // N3-N0
    ipv4.SetBase ("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if30 = ipv4.Assign (d30);

    // Populate OSPF Link-State Database
    // OSPF in ns-3 will converge on its own after interfaces are up

    // Schedule routing table printing after OSPF convergence (e.g., at 5s, 10s)
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
        Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (i), Seconds (5.0));
        Simulator::Schedule (Seconds (10.0), &PrintRoutingTable, nodes.Get (i), Seconds (10.0));
    }

    Simulator::Stop (Seconds (12.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}