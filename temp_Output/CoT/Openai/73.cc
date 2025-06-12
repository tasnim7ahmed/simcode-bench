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

    // Create nodes for routers A, B, and C
    NodeContainer nodes;
    nodes.Create(3); // 0: A, 1: B, 2: C

    // Create point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link A-B
    NodeContainer ab = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev_ab = p2p.Install(ab);

    // Link B-C
    NodeContainer bc = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev_bc = p2p.Install(bc);

    // Install the internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    // A-B: x.x.x.0/30 (use 10.1.1.0/30)
    Ipv4AddressHelper ipv4_ab;
    ipv4_ab.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer iface_ab = ipv4_ab.Assign(dev_ab);

    // B-C: y.y.y.0/30 (use 10.1.2.0/30)
    Ipv4AddressHelper ipv4_bc;
    ipv4_bc.SetBase("10.1.2.0", "255.255.255.252");
    Ipv4InterfaceContainer iface_bc = ipv4_bc.Assign(dev_bc);

    // Assign /32 loopback addresses for A and C
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeC = nodes.Get(2);
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();

    int32_t loIfA = ipv4A->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4A->AddAddress(loIfA, Ipv4InterfaceAddress("10.0.0.1", "255.255.255.255"));
    ipv4A->SetMetric(loIfA, 1);
    ipv4A->SetUp(loIfA);

    int32_t loIfC = ipv4C->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4C->AddAddress(loIfC, Ipv4InterfaceAddress("10.0.0.2", "255.255.255.255"));
    ipv4C->SetMetric(loIfC, 1);
    ipv4C->SetUp(loIfC);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP packet sink on router C, listening on port 5000
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(Ipv4Address("10.0.0.2"), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install OnOff application on router A to send to router C
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer clientApp = onoff.Install(nodeA);

    // Enable pcap tracing on all point-to-point devices
    p2p.EnablePcapAll("three-router-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}