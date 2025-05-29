#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
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

    // Point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link A-B
    NodeContainer ab(nodeA, nodeB);
    NetDeviceContainer devAB = p2p.Install(ab);

    // Link B-C
    NodeContainer bc(nodeB, nodeC);
    NetDeviceContainer devBC = p2p.Install(bc);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses: /32 for each interface
    Ipv4AddressHelper ipv4;
    // A-B link
    ipv4.SetBase("10.1.1.1", "255.255.255.255");
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);
    Ipv4Address addrA = ifAB.GetAddress(0);
    Ipv4Address addrB1 = ifAB.GetAddress(1);

    // B-C link
    ipv4.SetBase("10.1.2.1", "255.255.255.255");
    Ipv4InterfaceContainer ifBC = ipv4.Assign(devBC);
    Ipv4Address addrB2 = ifBC.GetAddress(0);
    Ipv4Address addrC = ifBC.GetAddress(1);

    // Set up static routing
    // Node A: route to C via B's address on A-B link
    Ipv4StaticRoutingHelper staticRoutingHelper;

    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticA = staticRoutingHelper.GetStaticRouting(ipv4A);
    staticA->AddHostRouteTo(addrC, addrB1, 1);

    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticB = staticRoutingHelper.GetStaticRouting(ipv4B);
    // On B: route to C via BC link
    staticB->AddHostRouteTo(addrC, addrC, 2);
    // On B: route to A via AB link
    staticB->AddHostRouteTo(addrA, addrA, 1);

    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticC = staticRoutingHelper.GetStaticRouting(ipv4C);
    // Node C: route to A via B's address on BC link
    staticC->AddHostRouteTo(addrA, addrB2, 1);

    // Set up OnOffApplication on node A to send UDP to node C
    uint16_t port = 5000;
    OnOffHelper onOff("ns3::UdpSocketFactory", Address(InetSocketAddress(addrC, port)));
    onOff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    onOff.SetAttribute("PacketSize", UintegerValue(1024));
    onOff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onOff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer appA = onOff.Install(nodeA);

    // PacketSink on node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appC = sink.Install(nodeC);
    appC.Start(Seconds(0.0));
    appC.Stop(Seconds(11.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    // Enable PCAP
    p2p.EnablePcapAll("three-router", true);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}