#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterTopology");

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
    Ipv4InterfaceContainer interfacesAB, interfacesBC;

    address.SetBase("10.1.1.0", "255.255.255.252");
    interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.252");
    interfacesBC = address.Assign(devicesBC);

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingA = routingHelper.GetStaticRouting(ipv4A);
    routingA->AddHostRouteTo(Ipv4Address("10.1.2.2"), Ipv4Address("10.1.1.2"), 1);

    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routingB = routingHelper.GetStaticRouting(ipv4B);
    routingB->AddHostRouteTo(Ipv4Address("10.1.2.2"), 1); 

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfacesBC.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    p2p.EnablePcapAll("three-router-topology");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}