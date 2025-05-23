#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for relevant components to observe behavior
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL); // For routing details
    LogComponentEnable("Ipv4RoutingTable", LOG_LEVEL_ALL); // To see routing table contents

    // Configure common point-to-point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // 1. Create three nodes: A, B, and C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // 2. Set up point-to-point links between A-B and B-C
    // Link A-B
    NetDeviceContainer devicesAB;
    devicesAB = pointToPoint.Install(nodeA, nodeB);

    // Link B-C
    NetDeviceContainer devicesBC;
    devicesBC = pointToPoint.Install(nodeB, nodeC);

    // 3. Install Internet Stack on all nodes
    // Use Ipv4ListRoutingHelper to combine static and global routing.
    // Static routing (priority 0) will be preferred over global routing (priority 10).
    InternetStackHelper internet;
    Ipv4ListRoutingHelper listRoutingHelper;
    listRoutingHelper.Add(Ipv4StaticRoutingHelper(), 0); // Static routing, higher priority
    listRoutingHelper.Add(Ipv4GlobalRoutingHelper(), 10); // Global routing, lower priority
    internet.SetRoutingHelper(listRoutingHelper);
    internet.Install(nodes);

    // 4. Assign IP Addresses to interfaces
    Ipv4AddressHelper ipv4;

    // Network for A-B link: 10.1.1.0/24
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = ipv4.Assign(devicesAB);
    Ipv4Address nodeA_ip = interfacesAB.GetAddress(0); // Node A's IP: 10.1.1.1
    Ipv4Address nodeB_ab_ip = interfacesAB.GetAddress(1); // Node B's IP on A-B link: 10.1.1.2

    // Network for B-C link: 10.1.2.0/24
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = ipv4.Assign(devicesBC);
    Ipv4Address nodeB_bc_ip = interfacesBC.GetAddress(0); // Node B's IP on B-C link: 10.1.2.2
    Ipv4Address nodeC_ip = interfacesBC.GetAddress(1); // Node C's IP: 10.1.2.1

    // 5. Populate Global Routing Tables
    // This will set up routes for the directly connected /24 networks.
    // The static routes added subsequently will offer more specific /32 routes.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Inject static host routes at Node B
    // This satisfies the "inject routes at node B" and "static routing" requirements.
    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> routingProtocolB = ipv4B->GetRoutingProtocol();
    Ptr<Ipv4ListRouting> listRoutingB = DynamicCast<Ipv4ListRouting>(routingProtocolB);
    Ptr<Ipv4StaticRouting> staticRoutingB = listRoutingB->GetStaticRoutingProtocol();

    // Add explicit /32 host routes for Node A and Node C at Node B.
    // For directly connected hosts, the gateway is the host itself.
    staticRoutingB->AddHostRouteTo(nodeA_ip, nodeA_ip); // Route to Node A's IP (10.1.1.1/32)
    staticRoutingB->AddHostRouteTo(nodeC_ip, nodeC_ip); // Route to Node C's IP (10.1.2.1/32)

    // 7. Implement an OnOff UDP application at Node A to send traffic to Node C
    uint16_t port = 9; // Discard port

    // Install UDP Server on Node C
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodeC);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install OnOff UDP Client on Node A
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(nodeC_ip, port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Send continuously
    onoff.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    onoff.SetAttribute("DataRate", StringValue("1Mbps")); // 1 Mbps data rate

    ApplicationContainer clientApps = onoff.Install(nodeA);
    clientApps.Start(Seconds(2.0)); // Start client after server is up
    clientApps.Stop(Seconds(9.0)); // Stop client before server shuts down

    // 8. Include packet tracing and PCAP capturing functionality
    // ASCII trace for all point-to-point devices
    pointToPoint.EnableAsciiAll("three-router-network");
    // PCAP trace for all point-to-point devices
    pointToPoint.EnablePcapAll("three-router-network");

    // Optional: Schedule printing of routing tables at a specific time
    // This allows verifying the routing table contents after global and static routes are set.
    Simulator::Schedule(Seconds(0.5), &Ipv4GlobalRoutingHelper::PrintRoutingTableAll);

    // Set simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}