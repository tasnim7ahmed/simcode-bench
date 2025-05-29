#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"
#include <iostream>

using namespace ns3;

void PrintRoutingTable(Ptr<Node> node, Time printTime, std::string nodeName)
{
    Ipv4StaticRoutingHelper helper;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    std::cout << "Time: " << Simulator::Now().GetSeconds() << "s Routing table of " << nodeName << std::endl;
    Ptr<Ipv4RoutingProtocol> routing = ipv4->GetRoutingProtocol();
    Ptr<Ipv4> ipv4obj = node->GetObject<Ipv4>();
    if (ipv4obj)
    {
        Ipv4RoutingTableEntry entry;
        for (uint32_t j = 0; j < ipv4obj->GetNInterfaces(); ++j)
        {
            uint32_t nRoutes = routing->GetNRoutes();
            for (uint32_t i = 0; i < nRoutes; ++i)
            {
                entry = routing->GetRoute(i);
                std::cout << entry.GetDest() << "\t"
                          << entry.GetGateway() << "\t"
                          << entry.GetInterface() << "\t"
                          << entry.GetMetric() << std::endl;
            }
        }
    }
    std::cout << "----------------------------------------" << std::endl;
    Simulator::Schedule(printTime, &PrintRoutingTable, node, printTime, nodeName);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("Ipv4GlobalRouting", LOG_INFO);

    // SeedManager::SetSeed(2);

    NodeContainer routers;
    routers.Create(3); // A=0, B=1, C=2

    // Links: A-B, B-C, C-A (triangle)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links
    NetDeviceContainer dAB = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer dBC = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer dCA = p2p.Install(routers.Get(2), routers.Get(0));

    // Install the Internet stack with Distance Vector (RIP)
    RipHelper ripRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ripRouting, 0);

    InternetStackHelper stack;
    stack.SetRoutingHelper(listRouting);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iAB = ipv4.Assign(dAB);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iBC = ipv4.Assign(dBC);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iCA = ipv4.Assign(dCA);

    // Enable global routing to populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Print out routing tables periodically
    Simulator::Schedule(Seconds(1.0), &PrintRoutingTable, routers.Get(0), Seconds(2.0), "A");
    Simulator::Schedule(Seconds(1.0), &PrintRoutingTable, routers.Get(1), Seconds(2.0), "B");
    Simulator::Schedule(Seconds(1.0), &PrintRoutingTable, routers.Get(2), Seconds(2.0), "C");

    // Wait for RIP to converge
    Simulator::Schedule(Seconds(25.0), &PrintRoutingTable, routers.Get(0), Seconds(2.0), "A");
    Simulator::Schedule(Seconds(25.0), &PrintRoutingTable, routers.Get(1), Seconds(2.0), "B");
    Simulator::Schedule(Seconds(25.0), &PrintRoutingTable, routers.Get(2), Seconds(2.0), "C");

    // Simulate link break between B and C after 30 seconds
    Simulator::Schedule(Seconds(30.0), [&]{
        std::cout << "Breaking link B-C at " << Simulator::Now().GetSeconds() << "s" << std::endl;
        dBC.Get(0)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        dBC.Get(1)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        dBC.Get(0)->SetLinkDown();
        dBC.Get(1)->SetLinkDown();
    });

    // Print tables after the link break
    Simulator::Schedule(Seconds(35.0), &PrintRoutingTable, routers.Get(0), Seconds(2.0), "A");
    Simulator::Schedule(Seconds(35.0), &PrintRoutingTable, routers.Get(1), Seconds(2.0), "B");
    Simulator::Schedule(Seconds(35.0), &PrintRoutingTable, routers.Get(2), Seconds(2.0), "C");

    // Print tables periodically to observe "count to infinity"
    for (double t = 40.0; t <= 90.0; t += 10.0)
    {
        Simulator::Schedule(Seconds(t), &PrintRoutingTable, routers.Get(0), Seconds(2.0), "A");
        Simulator::Schedule(Seconds(t), &PrintRoutingTable, routers.Get(1), Seconds(2.0), "B");
        Simulator::Schedule(Seconds(t), &PrintRoutingTable, routers.Get(2), Seconds(2.0), "C");
    }

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}