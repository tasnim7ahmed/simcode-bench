#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"

#include <vector>

using namespace ns3;

int
main(int argc, char* argv[])
{
    // Enable logging for ICMPv6 and IPv6 routing
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL); // To see routing header processing
    LogComponentEnable("Ipv6EndPoint", LOG_LEVEL_ALL); // To see routing header processing
    LogComponentEnable("Ipv6RoutingProtocol", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer hosts;
    hosts.Create(2); // Host 0, Host 1
    Ptr<Node> h0 = hosts.Get(0);
    Ptr<Node> h1 = hosts.Get(1);

    NodeContainer routers;
    routers.Create(4); // Router 0, Router 1, Router 2, Router 3
    Ptr<Node> r0 = routers.Get(0);
    Ptr<Node> r1 = routers.Get(1);
    Ptr<Node> r2 = routers.Get(2);
    Ptr<Node> r3 = routers.Get(3);

    // Install internet stack (IPv6 only)
    InternetStackHelper ipv6Stack;
    ipv6Stack.SetIpv4StackInstall(false);
    ipv6Stack.Install(h0);
    ipv6Stack.Install(h1);
    ipv6Stack.Install(r0);
    ipv6Stack.Install(r1);
    ipv6Stack.Install(r2);
    ipv6Stack.Install(r3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv6AddressHelper ipv6;

    // H0-R0 link
    NetDeviceContainer dH0R0 = p2p.Install(h0, r0);
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iH0R0 = ipv6.Assign(dH0R0);
    Ipv6Address h0Addr = iH0R0.GetAddress(0, 0);     // H0's address
    Ipv6Address r0_H0R0_Ifc = iH0R0.GetAddress(1, 0); // R0's address on H0-R0 link

    // R0-R1 link
    NetDeviceContainer dR0R1 = p2p.Install(r0, r1);
    ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iR0R1 = ipv6.Assign(dR0R1);
    Ipv6Address r0_R0R1_Ifc = iR0R1.GetAddress(0, 0); // R0's address on R0-R1 link
    Ipv6Address r1_R0R1_Ifc = iR0R1.GetAddress(1, 0); // R1's address on R0-R1 link

    // R1-R2 link
    NetDeviceContainer dR1R2 = p2p.Install(r1, r2);
    ipv6.SetBase(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iR1R2 = ipv6.Assign(dR1R2);
    Ipv6Address r1_R1R2_Ifc = iR1R2.GetAddress(0, 0); // R1's address on R1-R2 link
    Ipv6Address r2_R1R2_Ifc = iR1R2.GetAddress(1, 0); // R2's address on R1-R2 link

    // R2-R3 link
    NetDeviceContainer dR2R3 = p2p.Install(r2, r3);
    ipv6.SetBase(Ipv6Address("2001:db8:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iR2R3 = ipv6.Assign(dR2R3);
    Ipv6Address r2_R2R3_Ifc = iR2R3.GetAddress(0, 0); // R2's address on R2-R3 link
    Ipv6Address r3_R2R3_Ifc = iR2R3.GetAddress(1, 0); // R3's address on R2-R3 link

    // R3-H1 link
    NetDeviceContainer dR3H1 = p2p.Install(r3, h1);
    ipv6.SetBase(Ipv6Address("2001:db8:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iR3H1 = ipv6.Assign(dR3H1);
    Ipv6Address r3_R3H1_Ifc = iR3H1.GetAddress(0, 0); // R3's address on R3-H1 link
    Ipv6Address h1Addr = iR3H1.GetAddress(1, 0);     // H1's address

    // Populate routing tables for all nodes
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // Set up Ping6 application from H0 to H1 with loose source routing
    Ping6Helper ping6;
    ping6.SetRemote(h1Addr); // Target address for H1

    // Define the loose source route: H0 -> R0 -> R1 -> R2 -> R3 -> H1
    // The addresses in the vector are the intermediate nodes (routers) that
    // the packet *must* visit, specified by their interface addresses that
    // are part of the path *beyond* the current hop.
    std::vector<Ipv6Address> looseSourceRoute;
    looseSourceRoute.push_back(r0_R0R1_Ifc); // Packet goes H0 -> R0. R0 forwards to R1 via R0-R1 interface
    looseSourceRoute.push_back(r1_R1R2_Ifc); // Packet goes R0 -> R1. R1 forwards to R2 via R1-R2 interface
    looseSourceRoute.push_back(r2_R2R3_Ifc); // Packet goes R1 -> R2. R2 forwards to R3 via R2-R3 interface
    looseSourceRoute.push_back(r3_R3H1_Ifc); // Packet goes R2 -> R3. R3 forwards to H1 via R3-H1 interface
    ping6.SetSourceRoute(looseSourceRoute);

    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Count", UintegerValue(4)); // Send 4 echo requests

    ApplicationContainer apps = ping6.Install(h0);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Enable tracing
    p2p.EnableAsciiAll(AsciiTraceHelper().CreateFileStream("loose-routing-ipv6.tr"));
    p2p.EnablePcapAll("loose-routing-ipv6");

    // Run simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}