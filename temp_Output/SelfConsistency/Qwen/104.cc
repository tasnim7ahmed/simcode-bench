#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterTopology");

int main(int argc, char *argv[]) {
    // Log components
    LogComponentEnable("ThreeRouterTopology", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);  // A (0), B (1), C (2)

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Links: A <-> B and B <-> C
    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    // Interfaces: A-B: 10.1.1.0/30, B-C: 10.1.2.0/30
    ipv4.SetBase("10.1.1.0", "255.255.255.252");  // /30 network for A-B
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);

    ipv4.SetBase("10.1.2.0", "255.255.255.252");  // /30 network for B-C
    Ipv4InterfaceContainer ifBC = ipv4.Assign(devBC);

    // Assign /32 IP address to node C's interface directly connected to B
    Ipv4Address addrC("192.168.1.1");
    uint32_t indexC = ifBC.GetAddress(1).Get();
    nodes.Get(2)->GetObject<Ipv4>()->AssignDynamicAddresses(addrC, Ipv4Mask::Get32());

    // Static routing
    Ipv4StaticRoutingHelper routingHelper;

    // Route from A to C via B
    Ptr<Ipv4StaticRouting> aRouting = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    aRouting->AddHostRouteTo(Ipv4Address("192.168.1.1"), Ipv4Address("10.1.1.2"), 1);  // via B

    // Route on B to forward packets to C
    Ptr<Ipv4StaticRouting> bRouting = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    bRouting->AddHostRouteTo(Ipv4Address("192.168.1.1"), Ipv4Address("10.1.2.2"), 2);  // via C

    // Setup applications
    uint16_t port = 9;  // Discard port

    // Packet sink on node C
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address("192.168.1.1"), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // OnOff application on node A
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(Ipv4Address("192.168.1.1"), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-topology.tr"));
    p2p.EnablePcapAll("three-router-topology");

    // Simulation run
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}