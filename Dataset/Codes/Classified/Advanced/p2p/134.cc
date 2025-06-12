#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PoissonReverseExample");

void
LogRouteTable(Ptr<Ipv4> ipv4)
{
    Ptr<Ipv4RoutingTableEntry> route;
    uint32_t numRoutes = ipv4->GetRoutingTableEntryCount();
    for (uint32_t i = 0; i < numRoutes; ++i)
    {
        route = ipv4->GetRoutingTableEntry(i);
        std::cout << "Destination: " << route->GetDestNetwork() << ", Gateway: " 
                  << route->GetGateway() << ", Interface: " << route->GetInterface() 
                  << std::endl;
    }
}

void
UpdateRoutes(Ptr<Ipv4StaticRouting> staticRouting, Ipv4Address dst, Ipv4Address nextHop, uint32_t cost)
{
    // Update the route using Poisson Reverse logic
    uint32_t reverseCost = 1 / cost; // Poisson reverse logic: invert cost

    staticRouting->SetDefaultRoute(nextHop, reverseCost);
    NS_LOG_UNCOND("Updated route with Poisson Reverse cost: " << reverseCost);
}

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("PoissonReverseExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install network stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Enable static routing
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting_0 = staticRoutingHelper.GetStaticRouting(ipv4_0);
    
    Ptr<Ipv4> ipv4_1 = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting_1 = staticRoutingHelper.GetStaticRouting(ipv4_1);

    // Set initial routes
    staticRouting_0->AddHostRouteTo(Ipv4Address("10.1.1.2"), Ipv4Address("10.1.1.1"), 1);
    staticRouting_1->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.1.2"), 1);

    // Log initial routing table
    LogRouteTable(ipv4_0);
    LogRouteTable(ipv4_1);

    // Update routes based on Poisson Reverse logic periodically
    Simulator::Schedule(Seconds(5), &UpdateRoutes, staticRouting_0, Ipv4Address("10.1.1.2"), Ipv4Address("10.1.1.1"), 1);
    Simulator::Schedule(Seconds(5), &UpdateRoutes, staticRouting_1, Ipv4Address("10.1.1.1"), Ipv4Address("10.1.1.2"), 1);

    // Create a simple TCP flow from node 0 to node 1
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

