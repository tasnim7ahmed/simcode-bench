#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoClientServerRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign(dev01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address.Assign(dev12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = address.Assign(dev23);

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_0 = routingHelper.GetStaticRouting(ipv4_0);
    routing_0->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"), 1);

    Ptr<Ipv4> ipv4_1 = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_1 = routingHelper.GetStaticRouting(ipv4_1);
    routing_1->AddHostRouteTo(Ipv4Address("10.1.3.1"), Ipv4Address("10.1.2.2"), 2);

    Ptr<Ipv4> ipv4_2 = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_2 = routingHelper.GetStaticRouting(ipv4_2);
    routing_2->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.2.1"), 1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(if23.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}