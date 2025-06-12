#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DvLoopSimulation");

// Utility to print routing table
void PrintRoutingTable(Ptr<Node> node, Time printTime, std::string nodeName)
{
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
    Ptr<Ipv4StaticRouting> sr = DynamicCast<Ipv4StaticRouting>(rp);
    std::ostringstream oss;
    oss << "Routing table of " << nodeName << " at " << printTime.GetSeconds() << "s:\n";
    ipv4->GetRoutingProtocol()->PrintRoutingTable(Create<OutputStreamWrapper>(&oss), Time::Now());
    NS_LOG_UNCOND(oss.str());
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("DvLoopSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2); // A:0, B:1

    // Create the third node as "Destination"
    Ptr<Node> destination = CreateObject<Node>();

    // Point-to-point link between A and B
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer abDevices = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Point-to-point link from A to Dest
    NetDeviceContainer adDevices = p2p.Install(nodes.Get(0), destination);

    // Point-to-point link from B to Dest
    NetDeviceContainer bdDevices = p2p.Install(nodes.Get(1), destination);

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install(nodes);
    stack.Install(destination);

    // Addressing
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIf = ipv4.Assign(abDevices);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer adIf = ipv4.Assign(adDevices);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bdIf = ipv4.Assign(bdDevices);

    // Manual routing: Use static routing to simulate initial DV convergence
    Ipv4StaticRoutingHelper staticRouting;
    // On Node A
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticA = staticRouting.GetStaticRouting(ipv4A);
    // A's direct LANs already picked up
    staticA->AddNetworkRouteTo(Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"), abIf.GetAddress(1), 1);

    // On Node B
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticB = staticRouting.GetStaticRouting(ipv4B);
    staticB->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), abIf.GetAddress(0), 1);

    // On Destination node
    Ptr<Ipv4> ipv4D = destination->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticD = staticRouting.GetStaticRouting(ipv4D);

    // A and B route to Dest via their directly connected links
    // Destination adds direct routes as usual (none needed for a simple IP host)

    // UDP Echo setup
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(destination);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(14.0));

    UdpEchoClientHelper echoClient(bdIf.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(14.0));

    // Show routing tables before failure
    Simulator::Schedule(Seconds(1.5), &PrintRoutingTable, nodes.Get(0), Seconds(1.5), "NodeA");
    Simulator::Schedule(Seconds(1.5), &PrintRoutingTable, nodes.Get(1), Seconds(1.5), "NodeB");

    // At t=5, simulate the link failure to Destination
    // By bringing down the A-D and B-D links, only A-B remain
    Simulator::Schedule(Seconds(5.0), [adDevices, bdDevices] {
        adDevices.Get(0)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        adDevices.Get(1)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        bdDevices.Get(0)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        bdDevices.Get(1)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
    });

    // At t=5.01, remove the direct route to destination from Node A and B (simulating DV's slow reaction)
    Simulator::Schedule(Seconds(5.01), [staticA, staticB] {
        // Remove existing direct routes to 10.0.3.0 (/24) and 10.0.2.0 (/24). In DV, this would happen after next update, but here we simulate it instantly.
        staticA->RemoveRoute(1); // Assumes route to 10.0.3.0 is at index 1
        staticB->RemoveRoute(1); // Assumes route to 10.0.2.0 is at index 1
    });

    // At t=5.02, add new "DV" route to destination (looped)
    Simulator::Schedule(Seconds(5.02), [staticA, staticB, abIf] {
        // On A, use B as next hop for destination network
        staticA->AddNetworkRouteTo(Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"), abIf.GetAddress(1), 1);
        // On B, use A as next hop for destination network
        staticB->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), abIf.GetAddress(0), 1);
    });

    // Print routing tables after DV loop formed
    Simulator::Schedule(Seconds(5.5), &PrintRoutingTable, nodes.Get(0), Seconds(5.5), "NodeA");
    Simulator::Schedule(Seconds(5.5), &PrintRoutingTable, nodes.Get(1), Seconds(5.5), "NodeB");

    // Enable pcap tracing on A-B link to observe packets looping
    p2p.EnablePcapAll("dv-loop");

    Simulator::Stop(Seconds(14.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}