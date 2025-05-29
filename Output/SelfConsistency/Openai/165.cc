/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-osspf-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfSquareTopologyExample");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    std::cout << "Routing table of node " << node->GetId () << " at " << printTime.GetSeconds () << "s:" << std::endl;
    if (ipv4)
    {
        Ipv4RoutingTableEntry route;
        for (uint32_t i = 0; i < ipv4->GetNInterfaces (); ++i)
        {
            for (uint32_t j = 0; ; ++j)
            {
                if (!ipv4->GetRoutingProtocol ()->GetRoute (j, route))
                {
                    break;
                }
                std::cout << route << std::endl;
            }
        }
    }
    std::cout << "-----------------------------" << std::endl;
}

int
main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    LogComponentEnable ("OspfSquareTopologyExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (4);

    // Naming nodes for clarity
    Names::Add ("Node0", nodes.Get (0));
    Names::Add ("Node1", nodes.Get (1));
    Names::Add ("Node2", nodes.Get (2));
    Names::Add ("Node3", nodes.Get (3));

    // Square topology (links: n0-n1, n1-n2, n2-n3, n3-n0)
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

    // Link 0: Node0 <--> Node1
    NodeContainer link0;
    link0.Add (nodes.Get (0));
    link0.Add (nodes.Get (1));
    NetDeviceContainer dev0 = csma.Install (link0);

    // Link 1: Node1 <--> Node2
    NodeContainer link1;
    link1.Add (nodes.Get (1));
    link1.Add (nodes.Get (2));
    NetDeviceContainer dev1 = csma.Install (link1);

    // Link 2: Node2 <--> Node3
    NodeContainer link2;
    link2.Add (nodes.Get (2));
    link2.Add (nodes.Get (3));
    NetDeviceContainer dev2 = csma.Install (link2);

    // Link 3: Node3 <--> Node0
    NodeContainer link3;
    link3.Add (nodes.Get (3));
    link3.Add (nodes.Get (0));
    NetDeviceContainer dev3 = csma.Install (link3);

    // Install Internet Stack with OSPF
    Ipv4OspfRoutingHelper ospfRouting;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add (ospfRouting, 10);
    list.Add (staticRouting, 0);

    InternetStackHelper internet;
    internet.SetRoutingHelper (list);
    internet.Install (nodes);

    // Assign IP addresses to each point-to-point link
    Ipv4AddressHelper ipv4;

    // Subnet 10.0.1.0/30: Node0 <-> Node1
    ipv4.SetBase ("10.0.1.0", "255.255.255.252");
    Ipv4InterfaceContainer iface0 = ipv4.Assign (dev0);

    // Subnet 10.0.2.0/30: Node1 <-> Node2
    ipv4.SetBase ("10.0.2.0", "255.255.255.252");
    Ipv4InterfaceContainer iface1 = ipv4.Assign (dev1);

    // Subnet 10.0.3.0/30: Node2 <-> Node3
    ipv4.SetBase ("10.0.3.0", "255.255.255.252");
    Ipv4InterfaceContainer iface2 = ipv4.Assign (dev2);

    // Subnet 10.0.4.0/30: Node3 <-> Node0
    ipv4.SetBase ("10.0.4.0", "255.255.255.252");
    Ipv4InterfaceContainer iface3 = ipv4.Assign (dev3);

    // Enable OSPF on all interfaces except loopback (interface 0)
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
        Ptr<Node> node = nodes.Get (i);
        Ptr<Ipv4> ipv4Obj = node->GetObject<Ipv4> ();
        for (uint32_t j = 1; j < ipv4Obj->GetNInterfaces (); ++j)
        {
            ospfRouting.SetInterfaceExclusions (node, j, false);
        }
    }

    // Populate routing tables (OSPF needs some time to converge)
    Simulator::Stop (Seconds (10.0));

    // Print the routing tables at t = 3, t = 7, and t = 10 seconds
    for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
        Simulator::Schedule (Seconds (3.0), &PrintRoutingTable, nodes.Get (i), Seconds(3.0));
        Simulator::Schedule (Seconds (7.0), &PrintRoutingTable, nodes.Get (i), Seconds(7.0));
        Simulator::Schedule (Seconds (10.0), &PrintRoutingTable, nodes.Get (i), Seconds(10.0));
    }

    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}