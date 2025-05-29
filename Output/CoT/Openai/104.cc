#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create 3 nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point links: A<->B and B<->C
    NodeContainer nA_nB = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer nB_nC = NodeContainer(nodes.Get(1), nodes.Get(2));

    // Configure point-to-point channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Install devices
    NetDeviceContainer devA_B = p2p.Install(nA_nB);
    NetDeviceContainer devB_C = p2p.Install(nB_nC);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign /32 IP addresses (host routes)
    Ipv4Address addrA("10.0.0.1");
    Ipv4Address addrB1("10.0.0.2");
    Ipv4Address addrB2("10.0.1.1");
    Ipv4Address addrC("10.0.1.2");

    Ipv4InterfaceContainer ifA_B, ifB_A, ifB_C, ifC_B;

    // Assign IP addresses for A<->B
    Ipv4AddressHelper ipA_B;
    ipA_B.SetBase("10.0.0.0", "255.255.255.255");
    ifA_B = ipA_B.Assign(devA_B);

    // Assign IP addresses for B<->C
    Ipv4AddressHelper ipB_C;
    ipB_C.SetBase("10.0.1.0", "255.255.255.255");
    ifB_C = ipB_C.Assign(devB_C);

    // Configure static routing
    Ipv4StaticRoutingHelper staticRouting;

    // Node A: route to 10.0.1.2 via 10.0.0.2
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticA = staticRouting.GetStaticRouting(ipv4A);
    staticA->AddHostRouteTo(Ipv4Address("10.0.1.2"), Ipv4Address("10.0.0.2"), 1);

    // Node B: route to 10.0.1.2 via 10.0.1.2 (direct on interface 2), and to 10.0.0.1 via 10.0.0.1
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticB = staticRouting.GetStaticRouting(ipv4B);
    // Route for B to C (directly attached on interface 2)
    staticB->AddHostRouteTo(Ipv4Address("10.0.1.2"), Ipv4Address("10.0.1.2"), 2);
    // Route for B to A (directly attached on interface 1)
    staticB->AddHostRouteTo(Ipv4Address("10.0.0.1"), Ipv4Address("10.0.0.1"), 1);

    // Node C: route to 10.0.0.1 via 10.0.1.1
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticC = staticRouting.GetStaticRouting(ipv4C);
    staticC->AddHostRouteTo(Ipv4Address("10.0.0.1"), Ipv4Address("10.0.1.1"), 1);

    // Setup UDP traffic from A to C (OnOff application)
    uint16_t port = 8000;

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    AddressValue remoteAddress(InetSocketAddress(Ipv4Address("10.0.1.2"), port));
    onoff.SetAttribute("Remote", remoteAddress);
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

    ApplicationContainer app = onoff.Install(nodes.Get(0));

    // PacketSink on C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable tracing and pcap
    p2p.EnablePcapAll("three-router-static");

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router-static.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}