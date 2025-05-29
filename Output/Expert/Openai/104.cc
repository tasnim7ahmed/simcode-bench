#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // A <-> B
    NodeContainer a_b(nodeA, nodeB);
    NetDeviceContainer devAB = p2p.Install(a_b);

    // B <-> C
    NodeContainer b_c(nodeB, nodeC);
    NetDeviceContainer devBC = p2p.Install(b_c);

    // Install Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses (use /32 on all interfaces)
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.255");
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);
    ipv4.SetBase("10.0.1.0", "255.255.255.255");
    Ipv4InterfaceContainer ifBC = ipv4.Assign(devBC);

    // PopulateRoutingTables will not help since we use static routing
    Ipv4StaticRoutingHelper staticRoutingHelper;

    // Static routing for A (to C via B)
    Ptr<Ipv4StaticRouting> aStaticRouting = staticRoutingHelper.GetStaticRouting(nodeA->GetObject<Ipv4>());
    aStaticRouting->AddHostRouteTo(
        ifBC.GetAddress(1), // Node C interface address
        ifAB.GetAddress(1), // Next hop: Node B interface as seen from A
        1                   // Interface index of A->B
    );

    // Static routing for B
    Ptr<Ipv4StaticRouting> bStaticRouting = staticRoutingHelper.GetStaticRouting(nodeB->GetObject<Ipv4>());
    // Route to C from B
    bStaticRouting->AddHostRouteTo(
        ifBC.GetAddress(1), // Node C
        ifBC.GetAddress(1), // Directly connected
        2                   // Interface index B->C
    );
    // Route to A from B
    bStaticRouting->AddHostRouteTo(
        ifAB.GetAddress(0), // Node A
        ifAB.GetAddress(0), // Directly connected
        1                   // Interface index B->A
    );

    // Static routing for C (for reply traffic, optional)
    Ptr<Ipv4StaticRouting> cStaticRouting = staticRoutingHelper.GetStaticRouting(nodeC->GetObject<Ipv4>());
    cStaticRouting->AddHostRouteTo(
        ifAB.GetAddress(0), // Node A address
        ifBC.GetAddress(0), // Next hop: B's interface as seen from C
        1                   // Interface index C->B
    );

    // Set up OnOff Application on A to send UDP to C
    uint16_t port = 9000;
    Address sinkAddress(InetSocketAddress(ifBC.GetAddress(1), port));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer app = onoff.Install(nodeA);

    // PacketSink on C
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable tracing
    p2p.EnablePcapAll("three-router");
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}