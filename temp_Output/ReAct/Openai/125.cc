#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStarTopology");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
    Ipv4GlobalRoutingHelper g;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    std::ostringstream oss;
    ipv4->GetRoutingProtocol ()->Print (oss);
    std::ofstream osf;
    std::string fname = "node-" + std::to_string (node->GetId ()) + "-routing-table-" + std::to_string (printTime.GetSeconds()) + "s.log";
    osf.open (fname.c_str ());
    osf << "Time: " << printTime.GetSeconds() << "s\n";
    osf << oss.str();
    osf.close();
}

void
ScheduleRoutingTablePrint (std::vector<Ptr<Node> > routers, Time interval, Time stopTime)
{
    for (Time t = Seconds (0); t <= stopTime; t += interval)
    {
        for (uint32_t i = 0; i < routers.size (); ++i)
        {
            Simulator::Schedule (t, &PrintRoutingTable, routers[i], t);
        }
    }
}

int
main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);
    LogComponentEnable ("RipStarTopology", LOG_LEVEL_INFO);

    // Create nodes: One central router, four "edge" routers
    NodeContainer centralNode;
    centralNode.Create (1);

    NodeContainer edgeNodes;
    edgeNodes.Create (4);

    NodeContainer allRouters;
    allRouters.Add (centralNode.Get (0));
    for (uint32_t i = 0; i < edgeNodes.GetN (); ++i)
        allRouters.Add (edgeNodes.Get (i));

    // Create P2P links (central <-> edge)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    std::vector<NetDeviceContainer> p2pDevices (4);
    std::vector<NodeContainer> pairContainers (4);

    for (uint32_t i = 0; i < 4; ++i)
    {
        pairContainers[i] = NodeContainer (centralNode.Get (0), edgeNodes.Get (i));
        p2pDevices[i] = p2p.Install (pairContainers[i]);
    }

    // Install internet stack with RIP on all routers
    RipHelper ripRouting;
    ripRouting.ExcludeInterface (centralNode, 1);   // These will be set below per interface
    Ipv4ListRoutingHelper listRH;
    listRH.Add (ripRouting, 0);

    InternetStackHelper internet;
    internet.SetRoutingHelper (listRH);
    internet.Install (allRouters);

    // Assign IP addresses for each link
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces (4);

    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
        interfaces[i] = address.Assign (p2pDevices[i]);
    }

    // Assign loopback IPs (optional, for completeness)
    address.SetBase ("127.0.0.0", "255.0.0.0");
    for (uint32_t i = 0; i < allRouters.GetN (); ++i)
    {
        Ptr<Node> router = allRouters.Get (i);
        Ptr<NetDevice> loopback = router->GetDevice (router->GetNDevices() - 1);
        address.Assign (NetDeviceContainer (loopback));
    }

    // Enable RIP on all interfaces of routers, except loopback
    for (uint32_t i = 0; i < allRouters.GetN (); ++i)
    {
        Ptr<Ipv4> ipv4 = allRouters.Get (i)->GetObject<Ipv4> ();
        uint32_t nIfaces = ipv4->GetNInterfaces ();
        for (uint32_t j = 1; j < nIfaces; ++j)
        {
            ripRouting.SetInterfaceExclusion (allRouters.Get (i), j, false);
        }
    }

    // Enable routing table logging
    ScheduleRoutingTablePrint (std::vector<Ptr<Node> >{
        centralNode.Get (0),
        edgeNodes.Get (0),
        edgeNodes.Get (1),
        edgeNodes.Get (2),
        edgeNodes.Get (3)}, Seconds (5.0), Seconds (60.0));

    Simulator::Stop (Seconds (65.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}