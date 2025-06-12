#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoClientServerRouting");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between (0-1), (1-2), and (2-3)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    // Set routing for node 0 to node 3 via node 1 and node 2
    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> routingNode0 = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    routingNode0->AddHostRouteTo(Ipv4Address("10.1.3.2"), // server on node 3
                                 Ipv4Address("10.1.1.2"), // next hop is node 1
                                 1);                    // outgoing interface index

    // Node 1 routes from node 0 to node 2
    Ptr<Ipv4StaticRouting> routingNode1 = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    routingNode1->AddHostRouteTo(Ipv4Address("10.1.3.2"),
                                 Ipv4Address("10.1.2.2"), // next hop is node 2
                                 2);                    // outgoing interface index

    // Node 2 routes to node 3
    Ptr<Ipv4StaticRouting> routingNode2 = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    routingNode2->AddHostRouteTo(Ipv4Address("10.1.3.2"),
                                 Ipv4Address("10.1.3.1"), // directly connected
                                 2);                    // outgoing interface index

    // Setup UDP Echo Server on node 3
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces23.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet logging
    AsciiTraceHelper traceHelper;
    p2p.EnableAsciiAll(traceHelper.CreateFileStream("udp-echo-routing.tr"));
    p2p.EnablePcapAll("udp-echo-routing");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}