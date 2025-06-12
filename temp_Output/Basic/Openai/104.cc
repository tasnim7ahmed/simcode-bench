#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Point-to-point links attributes
    std::string dataRate = "10Mbps";
    std::string delay = "5ms";

    // Create point-to-point links: A-B, B-C
    NodeContainer nA_nB(nodeA, nodeB);
    NodeContainer nB_nC(nodeB, nodeC);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devA_B = p2p.Install(nA_nB);
    NetDeviceContainer devB_C = p2p.Install(nB_nC);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign /32 IP addresses
    Ipv4Address addrA("10.0.0.1");
    Ipv4Address addrB1("10.0.0.2");
    Ipv4Address addrB2("10.0.1.1");
    Ipv4Address addrC("10.0.1.2");
    Ipv4Mask mask32("255.255.255.255");

    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();

    Ipv4InterfaceContainer ifA_B, ifB_C;

    // Assign interfaces
    // devA_B.Get(0): nodeA's net device
    // devA_B.Get(1): nodeB's net device (toward A)
    Ipv4InterfaceAddress ifaceA_B_A(addrA, mask32);
    int32_t iA = ipv4A->AddInterface(devA_B.Get(0));
    ipv4A->AddAddress(iA, ifaceA_B_A);
    ipv4A->SetMetric(iA, 1);
    ipv4A->SetUp(iA);

    Ipv4InterfaceAddress ifaceA_B_B(addrB1, mask32);
    int32_t iB1 = ipv4B->AddInterface(devA_B.Get(1));
    ipv4B->AddAddress(iB1, ifaceA_B_B);
    ipv4B->SetMetric(iB1, 1);
    ipv4B->SetUp(iB1);

    // devB_C.Get(0): nodeB's net device (toward C)
    // devB_C.Get(1): nodeC's net device
    Ipv4InterfaceAddress ifaceB_C_B(addrB2, mask32);
    int32_t iB2 = ipv4B->AddInterface(devB_C.Get(0));
    ipv4B->AddAddress(iB2, ifaceB_C_B);
    ipv4B->SetMetric(iB2, 1);
    ipv4B->SetUp(iB2);

    Ipv4InterfaceAddress ifaceB_C_C(addrC, mask32);
    int32_t iC = ipv4C->AddInterface(devB_C.Get(1));
    ipv4C->AddAddress(iC, ifaceB_C_C);
    ipv4C->SetMetric(iC, 1);
    ipv4C->SetUp(iC);

    // Configure static routing
    Ipv4StaticRoutingHelper staticRoutingHelper;

    // On node A, route to C's /32 via B1's address
    Ptr<Ipv4StaticRouting> staticRoutingA = staticRoutingHelper.GetStaticRouting(ipv4A);
    staticRoutingA->AddHostRouteTo(addrC, addrB1, iA);

    // On node B, add host route to C via B2
    Ptr<Ipv4StaticRouting> staticRoutingB = staticRoutingHelper.GetStaticRouting(ipv4B);
    staticRoutingB->AddHostRouteTo(addrC, addrC, iB2); // Directly connected

    // On node B, add host route to A via B1
    staticRoutingB->AddHostRouteTo(addrA, addrA, iB1);

    // On node C, add host route to A via B2
    Ptr<Ipv4StaticRouting> staticRoutingC = staticRoutingHelper.GetStaticRouting(ipv4C);
    staticRoutingC->AddHostRouteTo(addrA, addrB2, iC);

    // Set up OnOff application on node A to C
    uint16_t port = 5000;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(addrC, port));
    onoff.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer appOnOff = onoff.Install(nodeA);

    // PacketSink on node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.ascii"));

    // Enable pcap tracing
    p2p.EnablePcapAll("three-router");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}