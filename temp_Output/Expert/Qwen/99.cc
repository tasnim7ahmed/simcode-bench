#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterGlobalRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB, devicesBC;
    devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    // Assign /32 host addresses to node A and node C
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    uint32_t idxA = ipv4A->AddInterface(devicesAB.Get(0));
    ipv4A->AddAddress(idxA, Ipv4InterfaceAddress(Ipv4Address("192.168.1.1"), Ipv4Mask("/32")));
    ipv4A->SetMetric(idxA, 1);
    ipv4A->SetUp(idxA);

    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    uint32_t idxC = ipv4C->AddInterface(devicesBC.Get(1));
    ipv4C->AddAddress(idxC, Ipv4InterfaceAddress(Ipv4Address("192.168.2.1"), Ipv4Mask("/32")));
    ipv4C->SetMetric(idxC, 1);
    ipv4C->SetUp(idxC);

    // Global routing setup
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Inject static route on Node B for transit
    Ptr<Ipv4StaticRouting> staticRoutingB = CreateObject<Ipv4StaticRouting>();
    Ipv4StaticRouting::Insert(staticRoutingB, 0);
    staticRoutingB->AddHostRouteTo(Ipv4Address("192.168.1.1"), interfacesAB.GetAddress(0), 1);
    staticRoutingB->AddHostRouteTo(Ipv4Address("192.168.2.1"), interfacesBC.GetAddress(1), 2);

    // Install applications
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("192.168.2.1"), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer appA = onoff.Install(nodes.Get(0));
    appA.Start(Seconds(1.0));
    appA.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appC = sink.Install(nodes.Get(2));
    appC.Start(Seconds(0.0));
    appC.Stop(Seconds(11.0));

    // Enable packet tracing and PCAP capture
    p2p.EnablePcapAll("three-router-global-routing");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}