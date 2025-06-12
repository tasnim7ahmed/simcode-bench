#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    uint16_t metric = 3; // Default metric for the n1-n3 link
    cmd.AddValue("metric", "Metric value for the n1-n3 link", metric);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4); // n0, n1, n2, n3

    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer n1n3 = NodeContainer(nodes.Get(1), nodes.Get(3));
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    PointToPointHelper p2p;

    // Link: n0 <-> n2
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    // Link: n1 <-> n2
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Link: n1 <-> n3
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(100)));
    NetDeviceContainer d1d3 = p2p.Install(n1n3);

    // Link: n2 <-> n3 (alternate path)
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = address.Assign(d1d3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    Ipv4StaticRoutingHelper routingHelper;

    // Set default routes manually using static routing
    Ptr<Ipv4> ipv4_n0 = nodes.Get(0)->GetObject<Ipv4>();
    Ipv4RoutingTableEntry route_n0_to_rest = Ipv4RoutingTableEntry::CreateNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), 1);
    ipv4_n0->GetRoutingProtocol()->NotifyAddRoute(route_n0_to_rest);

    // Manually configure routes on n3
    Ptr<Ipv4> ipv4_n3 = nodes.Get(3)->GetObject<Ipv4>();
    Ipv4RoutingTableEntry route_n3_to_n1_via_n2 = Ipv4RoutingTableEntry::CreateHostRouteTo(i1i2.GetAddress(1), i2i3.GetAddress(1), 1);
    ipv4_n3->GetRoutingProtocol()->NotifyAddRoute(route_n3_to_n1_via_n2);

    // Add alternate route with configurable metric via n1
    Ipv4RoutingTableEntry route_n3_to_n1_via_n1 = Ipv4RoutingTableEntry::CreateHostRouteTo(i1i3.GetAddress(1), i1i3.GetAddress(0), metric);
    ipv4_n3->GetRoutingProtocol()->NotifyAddRoute(route_n3_to_n1_via_n1);

    // UDP Echo Server on n1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on n3
    UdpEchoClientHelper echoClient(i1i2.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper traceHelper;
    p2p.EnableAsciiAll(traceHelper.CreateFileStream("network-topology-simulation.tr"));
    p2p.EnablePcapAll("network-topology-simulation");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}