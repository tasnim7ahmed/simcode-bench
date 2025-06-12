#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer routers;
    routers.Create(3); // A:0, B:1, C:2

    // Create point-to-point links: A-B and B-C
    NodeContainer ab = NodeContainer(routers.Get(0), routers.Get(1));
    NodeContainer bc = NodeContainer(routers.Get(1), routers.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devs_ab = p2p.Install(ab);
    NetDeviceContainer devs_bc = p2p.Install(bc);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(routers);

    // Assign IP addresses
    // A: 10.0.0.1/32,   B side of A-B: 192.168.0.2/30 (A:192.168.0.1/30, B:192.168.0.2/30)
    // B side of B-C: 192.168.1.1/30, C: 192.168.1.2/30
    // C: 10.0.1.1/32

    Ipv4AddressHelper address_ab;
    address_ab.SetBase("192.168.0.0", "255.255.255.252");
    Ipv4InterfaceContainer if_ab = address_ab.Assign(devs_ab);

    Ipv4AddressHelper address_bc;
    address_bc.SetBase("192.168.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if_bc = address_bc.Assign(devs_bc);

    // Assign /32 loopback addresses
    Ptr<Node> nodeA = routers.Get(0);
    Ptr<Node> nodeB = routers.Get(1);
    Ptr<Node> nodeC = routers.Get(2);

    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    int32_t loA = ipv4A->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4A->AddAddress(loA, Ipv4InterfaceAddress("10.0.0.1", "255.255.255.255"));
    ipv4A->SetMetric(loA, 1);
    ipv4A->SetUp(loA);

    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
    int32_t loC = ipv4C->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4C->AddAddress(loC, Ipv4InterfaceAddress("10.0.1.1", "255.255.255.255"));
    ipv4C->SetMetric(loC, 1);
    ipv4C->SetUp(loC);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure UDP traffic: OnOff from A (10.0.0.1) to C (10.0.1.1)
    uint16_t port = 5000;

    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(Ipv4Address("10.0.1.1"), port)));
    onoff.SetAttribute("DataRate", StringValue("3Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app = onoff.Install(nodeA);
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Set up PacketSink on C for UDP
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer sinkApp = sink.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("three-router");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}