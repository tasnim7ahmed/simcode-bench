#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable packet tracing and logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Name nodes for easier reference
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Create point-to-point links: A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // A-B link
    NodeContainer ab;
    ab.Add(nodeA);
    ab.Add(nodeB);
    NetDeviceContainer dev_ab = p2p.Install(ab);

    // B-C link
    NodeContainer bc;
    bc.Add(nodeB);
    bc.Add(nodeC);
    NetDeviceContainer dev_bc = p2p.Install(bc);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper addr_ab;
    addr_ab.SetBase("10.0.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if_ab = addr_ab.Assign(dev_ab);

    Ipv4AddressHelper addr_bc;
    addr_bc.SetBase("10.0.2.0", "255.255.255.252");
    Ipv4InterfaceContainer if_bc = addr_bc.Assign(dev_bc);

    // Assign /32 host addresses for A and C
    Ipv4Address hostA("192.168.1.1");
    Ipv4Address hostC("192.168.2.1");
    Ptr<Ipv4StaticRouting> staticRoutingA = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(nodeA->GetObject<Ipv4>()->GetRoutingProtocol());
    Ptr<Ipv4StaticRouting> staticRoutingC = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(nodeC->GetObject<Ipv4>()->GetRoutingProtocol());
    nodeA->GetObject<Ipv4>()->AddHostRouteTo(hostA, 1);
    nodeC->GetObject<Ipv4>()->AddHostRouteTo(hostC, 1);

    // Set host address on loopback for nodeA and nodeC
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
    uint32_t loA = 0; // loopback interface
    uint32_t loC = 0;
    ipv4A->AddAddress(loA, Ipv4InterfaceAddress(hostA, Ipv4Mask("255.255.255.255")));
    ipv4C->AddAddress(loC, Ipv4InterfaceAddress(hostC, Ipv4Mask("255.255.255.255")));
    ipv4A->SetMetric(loA, 1);
    ipv4C->SetMetric(loC, 1);
    ipv4A->SetUp(loA);
    ipv4C->SetUp(loC);

    // Configure static routing
    // At nodeA: route packets for hostC via nodeB
    staticRoutingA->AddHostRouteTo(hostC, if_ab.GetAddress(1), 1);

    // At nodeC: route packets for hostA via nodeB
    staticRoutingC->AddHostRouteTo(hostA, if_bc.GetAddress(0), 1);

    // At nodeB: inject host routes to A and C
    Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(nodeB->GetObject<Ipv4>()->GetRoutingProtocol());
    // To reach hostA via interface 1 (A-B link), out dev_ab.Get(1)
    staticRoutingB->AddHostRouteTo(hostA, if_ab.GetAddress(0), 1);
    // To reach hostC via interface 2 (B-C link), out dev_bc.Get(0)
    staticRoutingB->AddHostRouteTo(hostC, if_bc.GetAddress(1), 2);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install PacketSink on node C to receive UDP
    uint16_t port = 9999;
    Address sinkAddress(InetSocketAddress(hostC, port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(15.0));

    // Install OnOff application at nodeA, sending to hostC
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onoffApp = onoff.Install(nodeA);
    onoffApp.Start(Seconds(1.0));
    onoffApp.Stop(Seconds(14.0));

    // Enable PCAP tracing on all point-to-point devices
    p2p.EnablePcapAll("three-router");

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}