#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create 3 nodes: A,B,C
    NodeContainer nodes;
    nodes.Create(3);

    // Name nodes for clarity
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // NetDevice containers
    NetDeviceContainer dA_B = p2p.Install(nodeA, nodeB);
    NetDeviceContainer dB_C = p2p.Install(nodeB, nodeC);

    // Install stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper ipv4;

    // A-B subnet
    ipv4.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer iA_B = ipv4.Assign(dA_B);
    // nodeA: 10.0.0.1/30, nodeB: 10.0.0.2/30

    // B-C subnet
    ipv4.SetBase("10.0.0.4", "255.255.255.252");
    Ipv4InterfaceContainer iB_C = ipv4.Assign(dB_C);
    // nodeB: 10.0.0.5/30, nodeC: 10.0.0.6/30

    // Assign /32 host addresses to A and C's loopbacks
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();

    // Add loopback/host IP to A and C
    int32_t loIfA = ipv4A->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4A->AddAddress(loIfA, Ipv4InterfaceAddress("1.1.1.1", "255.255.255.255"));
    ipv4A->SetMetric(loIfA, 1);
    ipv4A->SetUp(loIfA);

    int32_t loIfC = ipv4C->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4C->AddAddress(loIfC, Ipv4InterfaceAddress("3.3.3.3", "255.255.255.255"));
    ipv4C->SetMetric(loIfC, 1);
    ipv4C->SetUp(loIfC);

    // Global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Add static routes at nodeB to handle host addresses
    Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ipv4B->GetRoutingProtocol());
    // Route to 1.1.1.1/32: via 10.0.0.1 on interface 1
    staticRoutingB->AddHostRouteTo(Ipv4Address("1.1.1.1"), Ipv4Address("10.0.0.1"), 1);
    // Route to 3.3.3.3/32: via 10.0.0.6 on interface 2
    staticRoutingB->AddHostRouteTo(Ipv4Address("3.3.3.3"), Ipv4Address("10.0.0.6"), 2);

    // OnOffApplication at nodeA to send UDP traffic to nodeC's 3.3.3.3:9999
    uint16_t port = 9999;

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("3.3.3.3"), port));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer app = onoff.Install(nodeA);

    // Sink app on nodeC to receive
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(12.0));

    // Enable tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));
    p2p.EnablePcapAll("three-router");

    // Enable packet capture
    // Already enabled above via p2p.EnablePcapAll

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}