#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/callback.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/rip-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinityRipSimulation");

int
main(int argc, char *argv[])
{
    // Log components
    LogComponentEnable("Ipv4RoutingTable", LOG_LEVEL_INFO);

    // Create nodes: 3 routers A, B, C
    NodeContainer routers;
    routers.Create(3);
    Ptr<Node> A = routers.Get(0);
    Ptr<Node> B = routers.Get(1);
    Ptr<Node> C = routers.Get(2);

    // Create point-to-point links: A<->B and B<->C (triangle)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // A<->B
    NetDeviceContainer ndcAB = p2p.Install(A, B);
    // B<->C
    NetDeviceContainer ndcBC = p2p.Install(B, C);
    // C<->A -- Required for triangle topology
    NetDeviceContainer ndcCA = p2p.Install(C, A);

    // Install Internet stack with RIP
    RipHelper ripRouting;
    ripRouting.ExcludeInterface(A, 1); // Exclude A's interface towards C from RIP passive interface
    ripRouting.ExcludeInterface(B, 2); // Exclude B's interface towards C from RIP passive interface
    ripRouting.ExcludeInterface(C, 1); // Exclude C's interface towards A from RIP passive interface

    Ipv4ListRoutingHelper listRH;
    listRH.Add(ripRouting, 0);

    InternetStackHelper stack;
    stack.SetRoutingHelper(listRH);
    stack.Install(routers);

    // Assign IPv4 addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = ipv4.Assign(ndcAB);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBC = ipv4.Assign(ndcBC);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifCA = ipv4.Assign(ndcCA);

    // Print routing tables at various times
    Ptr<Ipv4> ipv4A = A->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = B->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = C->GetObject<Ipv4>();

    Simulator::Schedule(Seconds(2.0), &Ipv4RoutingHelper::PrintRoutingTableAllAt, &ripRouting, Seconds(2.0), Create<OutputStreamWrapper>(&std::cout));
    Simulator::Schedule(Seconds(6.0), &Ipv4RoutingHelper::PrintRoutingTableAllAt, &ripRouting, Seconds(6.0), Create<OutputStreamWrapper>(&std::cout));
    Simulator::Schedule(Seconds(11.0), &Ipv4RoutingHelper::PrintRoutingTableAllAt, &ripRouting, Seconds(11.0), Create<OutputStreamWrapper>(&std::cout));
    Simulator::Schedule(Seconds(20.0), &Ipv4RoutingHelper::PrintRoutingTableAllAt, &ripRouting, Seconds(20.0), Create<OutputStreamWrapper>(&std::cout));

    // Break link B<->C at t = 5s
    Simulator::Schedule(Seconds(5.0), [&ndcBC]() {
        ndcBC.Get(0)->SetDown();
        ndcBC.Get(1)->SetDown();
        NS_LOG_UNCOND("Link B<->C is down at 5s");
    });

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}