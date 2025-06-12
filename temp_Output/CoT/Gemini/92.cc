#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceStaticRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_INFO);
    LogComponent::SetDefaultLogComponentEnable(true);

    NodeContainer nodes;
    nodes.Create(6); // Src, Rtr1, Rtr2, DestRtr, Dest

    NodeContainer srcRtr1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer srcRtr2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer rtr1DestRtr = NodeContainer(nodes.Get(1), nodes.Get(3));
    NodeContainer rtr2DestRtr = NodeContainer(nodes.Get(2), nodes.Get(3));
    NodeContainer destRtrDest = NodeContainer(nodes.Get(3), nodes.Get(4));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devSrcRtr1 = p2p.Install(srcRtr1);
    NetDeviceContainer devSrcRtr2 = p2p.Install(srcRtr2);
    NetDeviceContainer devRtr1DestRtr = p2p.Install(rtr1DestRtr);
    NetDeviceContainer devRtr2DestRtr = p2p.Install(rtr2DestRtr);
    NetDeviceContainer devDestRtrDest = p2p.Install(destRtrDest);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceSrcRtr1 = address.Assign(devSrcRtr1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceSrcRtr2 = address.Assign(devSrcRtr2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceRtr1DestRtr = address.Assign(devRtr1DestRtr);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceRtr2DestRtr = address.Assign(devRtr2DestRtr);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceDestRtrDest = address.Assign(devDestRtrDest);

    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routingRtr1 = staticRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    routingRtr1->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.2"), 1);

    Ptr<Ipv4StaticRouting> routingRtr2 = staticRouting.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    routingRtr2->AddNetworkRouteTo(Ipv4Address("10.1.5.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.4.2"), 1);

    Ptr<Ipv4StaticRouting> routingDestRtr = staticRouting.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    routingDestRtr->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.1"), 1);
    routingDestRtr->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.4.1"), 1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    // First flow: Src -> Rtr1 -> DestRtr -> Dest
    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(ifaceDestRtrDest.GetAddress(1), port));
    source1.SetAttribute("SendSize", UintegerValue(1024));
    source1.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps1 = source1.Install(nodes.Get(0));
    sourceApps1.Start(Seconds(1.0));
    sourceApps1.Stop(Seconds(5.0));

    PacketSinkHelper sink1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps1 = sink1.Install(nodes.Get(4));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(10.0));

    // Second flow: Src -> Rtr2 -> DestRtr -> Dest
    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(ifaceDestRtrDest.GetAddress(1), port));
    source2.SetAttribute("SendSize", UintegerValue(1024));
    source2.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps2 = source2.Install(nodes.Get(0));
    sourceApps2.Start(Seconds(6.0));
    sourceApps2.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}