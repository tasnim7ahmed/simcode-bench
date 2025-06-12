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
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Create point-to-point links: A-B and B-C
    NodeContainer ab(nodeA, nodeB);
    NodeContainer bc(nodeB, nodeC);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devAB = p2p.Install(ab);
    NetDeviceContainer devBC = p2p.Install(bc);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign /30 subnets for point-to-point links
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.252");
    Ipv4InterfaceContainer ifaceAB = ipv4.Assign(devAB);

    ipv4.SetBase("10.0.2.0", "255.255.255.252");
    Ipv4InterfaceContainer ifaceBC = ipv4.Assign(devBC);

    // Assign /32 host addresses to A and C (can use loopback)
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
    int32_t loopbackIfA = ipv4A->AddInterface(CreateObject<LoopbackNetDevice>());
    int32_t loopbackIfC = ipv4C->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4A->AddAddress(loopbackIfA, Ipv4InterfaceAddress("192.168.1.1", "255.255.255.255"));
    ipv4A->SetUp(loopbackIfA);
    ipv4C->AddAddress(loopbackIfC, Ipv4InterfaceAddress("192.168.1.2", "255.255.255.255"));
    ipv4C->SetUp(loopbackIfC);

    // Enable static and global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Add static host route for node A to reach C via B
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingB = staticRoutingHelper.GetStaticRouting(nodeB->GetObject<Ipv4>());

    // Node B: host routes for A and C via appropriate interfaces
    staticRoutingB->AddHostRouteTo(
        Ipv4Address("192.168.1.1"),       // Dest
        ifaceAB.GetAddress(0),            // Next hop (A)
        1                                 // Interface index (AB link on B)
    );

    staticRoutingB->AddHostRouteTo(
        Ipv4Address("192.168.1.2"),       // Dest
        ifaceBC.GetAddress(1),            // Next hop (C)
        2                                 // Interface index (BC link on B)
    );

    // Node A: add specific /32 host route to C via B
    Ptr<Ipv4StaticRouting> staticRoutingA = staticRoutingHelper.GetStaticRouting(nodeA->GetObject<Ipv4>());
    staticRoutingA->AddHostRouteTo(
        Ipv4Address("192.168.1.2"),
        ifaceAB.GetAddress(1),
        1
    );

    // Node C: add /32 host route to A via B
    Ptr<Ipv4StaticRouting> staticRoutingC = staticRoutingHelper.GetStaticRouting(nodeC->GetObject<Ipv4>());
    staticRoutingC->AddHostRouteTo(
        Ipv4Address("192.168.1.1"),
        ifaceBC.GetAddress(0),
        1
    );

    // Install OnOff UDP application on node A (dest: C)
    uint16_t sinkPort = 5000;
    Address sinkAddress(InetSocketAddress(Ipv4Address("192.168.1.2"), sinkPort));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(6.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(5.0)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApp = onoff.Install(nodeA);

    // Enable PCAP tracing
    p2p.EnablePcapAll("three-router");

    // Enable optional ASCII tracing
    // AsciiTraceHelper ascii;
    // p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    Simulator::Stop(Seconds(6.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}