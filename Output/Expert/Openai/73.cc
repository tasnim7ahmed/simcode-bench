#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer routers;
    routers.Create(3);

    // Names for readability
    Ptr<Node> routerA = routers.Get(0);
    Ptr<Node> routerB = routers.Get(1);
    Ptr<Node> routerC = routers.Get(2);

    // Point-to-point links
    NodeContainer ab = NodeContainer(routerA, routerB);
    NodeContainer bc = NodeContainer(routerB, routerC);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ndc_ab = p2p.Install(ab);
    NetDeviceContainer ndc_bc = p2p.Install(bc);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(routers);

    // Assign IP addresses
    // A "host" /32, subnet x.x.x.0/30 between A & B, subnet y.y.y.0/30 between B & C, C "host" /32
    // We'll pick: 10.0.0.1/32 (A), 10.0.0.0/30 (A-B), 10.0.0.2/32 (B)
    // 10.0.0.4/30 (B-C), 10.0.0.5/32 (C)

    // Subnet for A-B: 10.0.0.0/30
    Ipv4AddressHelper ipv4_ab;
    ipv4_ab.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer iface_ab = ipv4_ab.Assign(ndc_ab);

    // Subnet for B-C: 10.0.0.4/30
    Ipv4AddressHelper ipv4_bc;
    ipv4_bc.SetBase("10.0.0.4", "255.255.255.252");
    Ipv4InterfaceContainer iface_bc = ipv4_bc.Assign(ndc_bc);

    // Assign /32 host addresses to router A and C (loopback)
    Ptr<Ipv4> ipv4A = routerA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = routerC->GetObject<Ipv4>();
    int32_t loA = ipv4A->AddInterface(routerA->GetDevice(0));
    ipv4A->AddAddress(loA, Ipv4InterfaceAddress("192.168.1.1", "255.255.255.255"));
    ipv4A->SetMetric(loA, 1);
    ipv4A->SetUp(loA);

    int32_t loC = ipv4C->AddInterface(routerC->GetDevice(0));
    ipv4C->AddAddress(loC, Ipv4InterfaceAddress("192.168.3.1", "255.255.255.255"));
    ipv4C->SetMetric(loC, 1);
    ipv4C->SetUp(loC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    uint16_t sinkPort = 5000;
    Address sinkAddress(InetSocketAddress("192.168.3.1", sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(routerC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer onoffApp = onoff.Install(routerA);

    // Packet tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));
    p2p.EnablePcapAll("three-router");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}