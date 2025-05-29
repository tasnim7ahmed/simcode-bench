#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging/tracing
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create router/nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Set up point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // A <-> B
    NodeContainer ab(nodeA, nodeB);
    NetDeviceContainer devAB = p2p.Install(ab);

    // B <-> C
    NodeContainer bc(nodeB, nodeC);
    NetDeviceContainer devBC = p2p.Install(bc);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign /32 IPs to each interface
    Ipv4AddressHelper ipAB;
    ipAB.SetBase("10.0.1.0", "255.255.255.255");
    Ipv4InterfaceContainer ifAB = ipAB.Assign(devAB);

    Ipv4AddressHelper ipBC;
    ipBC.SetBase("10.0.2.0", "255.255.255.255");
    Ipv4InterfaceContainer ifBC = ipBC.Assign(devBC);

    // Enable packet capture
    p2p.EnablePcapAll("three-router");

    // Configure static routing: A to C via B (A->B->C)
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();

    Ptr<Ipv4StaticRouting> staticA = staticRouting.GetStaticRouting(ipv4A);
    Ptr<Ipv4StaticRouting> staticB = staticRouting.GetStaticRouting(ipv4B);
    Ptr<Ipv4StaticRouting> staticC = staticRouting.GetStaticRouting(ipv4C);

    // Node A: route to C's interface via B's interface
    staticA->AddHostRouteTo(
        ifBC.GetAddress(1), // C's BC interface
        ifAB.GetAddress(1)  // B's AB interface
    );

    // Node B: route to C's BC interface via B's BC interface (local), and to A's AB interface via B's AB interface (local)
    staticB->AddHostRouteTo(
        ifBC.GetAddress(1), // C's BC interface
        ifBC.GetAddress(1)  // directly connected
    );
    staticB->AddHostRouteTo(
        ifAB.GetAddress(0), // A's AB interface
        ifAB.GetAddress(0)  // directly connected
    );

    // Node C: for reverse traffic (optional if wanted)
    staticC->AddHostRouteTo(
        ifAB.GetAddress(0), // A's AB interface
        ifBC.GetAddress(0)  // B's BC interface
    );

    // OnOff Application on A -> C, UDP
    uint16_t sinkPort = 9999;
    Address sinkAddress(InetSocketAddress(ifBC.GetAddress(1), sinkPort));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onoffApp = onoff.Install(nodeA);
    onoffApp.Start(Seconds(1.0));
    onoffApp.Stop(Seconds(10.0));

    // Packet Sink on C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sink.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(11.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}