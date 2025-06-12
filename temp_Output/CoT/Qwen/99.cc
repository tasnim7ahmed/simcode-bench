#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterGlobalRouting");

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
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.252");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    Ipv4Address nodeAHostAddr = "192.168.1.1";
    Ipv4Address nodeCHostAddr = "192.168.2.1";

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingA = routingHelper.GetStaticRouting(ipv4A);
    routingA->AddHostRouteTo(nodeCHostAddr, interfacesAB.GetAddress(1), 1);

    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingC = routingHelper.GetStaticRouting(ipv4C);
    routingC->AddHostRouteTo(nodeAHostAddr, interfacesBC.GetAddress(1), 1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(nodeCHostAddr, 9));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-global-routing.tr"));
    p2p.EnablePcapAll("three-router-global-routing");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}