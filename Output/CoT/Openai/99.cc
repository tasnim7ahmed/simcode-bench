#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes: 0 = A, 1 = B, 2 = C
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point links between A-B and B-C
    NodeContainer ab = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer bc = NodeContainer(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_ab = p2p.Install(ab);
    NetDeviceContainer d_bc = p2p.Install(bc);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper addr;
    // AB link
    addr.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer i_ab = addr.Assign(d_ab);
    // BC link
    addr.SetBase("10.0.0.4", "255.255.255.252");
    Ipv4InterfaceContainer i_bc = addr.Assign(d_bc);

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();

    Ipv4Address ipA = i_ab.GetAddress(0);
    Ipv4Address ipBA = i_ab.GetAddress(1); // B's address on AB
    Ipv4Address ipBC = i_bc.GetAddress(0); // B's address on BC
    Ipv4Address ipC = i_bc.GetAddress(1);

    // Assign /32 host routes to A and C
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> srtA = staticRouting.GetStaticRouting(ipv4A);
    Ptr<Ipv4StaticRouting> srtB = staticRouting.GetStaticRouting(ipv4B);
    Ptr<Ipv4StaticRouting> srtC = staticRouting.GetStaticRouting(ipv4C);

    // Node A: host route to itself for its own address
    srtA->AddHostRouteTo(ipA, 1); // local loopback interface

    // Node C: host route to itself for its own address
    srtC->AddHostRouteTo(ipC, 1);

    // Node B: host route for A and C
    srtB->AddHostRouteTo(ipA, 1); // local on AB, interface 1 (index 1)
    srtB->AddHostRouteTo(ipC, 2); // local on BC, interface 2

    // Node B: default route for transit (just to demonstrate transit)
    // Actually, global routing can populate this, but let's inject a static route from A to C via B

    // Node A: add a route to C via B's AB address
    srtA->AddHostRouteTo(ipC, ipBA, 1);

    // Node C: add a route to A via B's BC address
    srtC->AddHostRouteTo(ipA, ipBC, 1);

    // Use global routing to handle all other routing table computation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install OnOffApplication on A to send to C
    uint16_t port = 8888;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ipC, port));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

    ApplicationContainer app = onoff.Install(nodes.Get(0));

    // Sink on node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("three-router");

    // Enable packet tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}