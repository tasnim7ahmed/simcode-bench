#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    Config::SetDefault("ns3::Ipv4L3Protocol::FragmentOnSend", BooleanValue(false));

    // Create three nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> A = nodes.Get(0);
    Ptr<Node> B = nodes.Get(1);
    Ptr<Node> C = nodes.Get(2);

    // Configure PointToPointHelper with specified data rate and delay
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install net devices for A-B link
    NetDeviceContainer devicesAB;
    devicesAB = p2pHelper.Install(A, B);

    // Install net devices for B-C link
    NetDeviceContainer devicesBC;
    devicesBC = p2pHelper.Install(B, C);

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to A-B link (e.g., 10.1.1.0/30)
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfacesAB;
    interfacesAB = address.Assign(devicesAB);

    // Assign IP addresses to B-C link (e.g., 10.1.2.0/30)
    address.SetBase("10.1.2.0", "255.255.255.252");
    Ipv4InterfaceContainer interfacesBC;
    interfacesBC = address.Assign(devicesBC);

    // Get IP addresses for static routing and application configuration
    Ipv4Address ipA = interfacesAB.GetAddress(0);    // A's IP on A-B link (e.g., 10.1.1.1)
    Ipv4Address ipB_AB = interfacesAB.GetAddress(1); // B's IP on A-B link (e.g., 10.1.1.2)
    Ipv4Address ipB_BC = interfacesBC.GetAddress(0); // B's IP on B-C link (e.g., 10.1.2.1)
    Ipv4Address ipC = interfacesBC.GetAddress(1);    // C's IP on B-C link (e.g., 10.1.2.2)

    // Configure Static Routing
    Ipv4StaticRoutingHelper staticRoutingHelper;

    // Get the Ipv4StaticRouting protocol for node A
    Ptr<Ipv4> ipv4A = A->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingA = staticRoutingHelper.Get(ipv4A->GetRoutingProtocol());
    
    // Get the Ipv4StaticRouting protocol for node C
    Ptr<Ipv4> ipv4C = C->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingC = staticRoutingHelper.Get(ipv4C->GetRoutingProtocol());

    // Add static route on Node A to reach Node C (ipC /32) through Node B (ipB_AB)
    staticRoutingA->AddHostRouteTo(ipC, ipB_AB);

    // Add static route on Node C to reach Node A (ipA /32) through Node B (ipB_BC)
    staticRoutingC->AddHostRouteTo(ipA, ipB_BC);

    // Set up OnOff application on Node A to send UDP packets to Node C
    uint16_t port = 9; // Echo port for PacketSink
    OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(ipC, port));
    onoffHelper.SetConstantRate(DataRate("1Mbps"));
    onoffHelper.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    ApplicationContainer onoffApps = onoffHelper.Install(A);
    onoffApps.Start(Seconds(1.0)); // Start sending at 1 second
    onoffApps.Stop(Seconds(9.0));  // Stop sending at 9 seconds

    // Set up PacketSink application on Node C to receive packets
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(C);
    sinkApps.Start(Seconds(0.0));  // Start receiving immediately
    sinkApps.Stop(Seconds(10.0)); // Stop receiving at 10 seconds

    // Enable tracing and packet capture
    p2pHelper.EnableAsciiAll("static-routing-three-routers");
    p2pHelper.EnablePcapAll("static-routing-three-routers");

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Clean up
    Simulator::Destroy();

    return 0;
}