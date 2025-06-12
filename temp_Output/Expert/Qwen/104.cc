#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterSimulation");

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

    NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesAB, interfacesBC;

    address.SetBase("10.1.1.0", "255.255.255.252");
    interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.252");
    interfacesBC = address.Assign(devicesBC);

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4StaticRouting> routeA = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    routeA->AddHostRouteTo(Ipv4Address("10.1.2.2"), Ipv4Address("10.1.1.2"), 1);

    Ptr<Ipv4StaticRouting> routeC = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    routeC->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.2.1"), 1);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesBC.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer appA = onoff.Install(nodes.Get(0));
    appA.Start(Seconds(1.0));
    appA.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appC = sink.Install(nodes.Get(2));
    appC.Start(Seconds(0.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router-sim.tr"));
    p2p.EnablePcapAll("three-router-sim");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}