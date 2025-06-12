#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodesA, nodesB, nodesC;
    Ptr<Node> routerA = CreateObject<Node>();
    Ptr<Node> routerB = CreateObject<Node>();
    Ptr<Node> routerC = CreateObject<Node>();

    nodesA.Add(routerA);
    nodesB.Add(routerB);
    nodesC.Add(routerC);

    PointToPointHelper p2p1, p2p2;
    p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB, devicesBC;
    devicesAB = p2p1.Install(routerA, routerB);
    devicesBC = p2p2.Install(routerB, routerC);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252"); // subnet for A-B link
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.252"); // subnet for B-C link
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer appA = onoff.Install(routerA);
    appA.Start(Seconds(1.0));
    appA.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer appC = sink.Install(routerC);
    appC.Start(Seconds(0.0));

    AsciiTraceHelper ascii;
    p2p1.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));
    p2p2.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}