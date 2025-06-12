#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoClientServerRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[5];
    for (uint32_t i = 0; i < 5; ++i) {
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[5];
    std::ostringstream subnet;
    for (uint32_t i = 0; i < 5; ++i) {
        subnet.str("");
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> staticRouting;

    // Setup routing from node 0 to node 5
    staticRouting = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    staticRouting->AddHostRouteTo(Ipv4Address("10.1.5.2"), 1);

    staticRouting = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    staticRouting->AddHostRouteTo(Ipv4Address("10.1.5.2"), 2);

    staticRouting = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    staticRouting->AddHostRouteTo(Ipv4Address("10.1.5.2"), 2);

    staticRouting = routingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    staticRouting->AddHostRouteTo(Ipv4Address("10.1.5.2"), 2);

    staticRouting = routingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    staticRouting->AddHostRouteTo(Ipv4Address("10.1.5.2"), 2);

    // Server on node 5
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on node 0
    UdpEchoClientHelper echoClient(interfaces[4].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}