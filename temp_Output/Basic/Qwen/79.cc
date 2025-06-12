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
    uint16_t metric = 3; // Default metric for link n1-n3
    cmd.AddValue("metric", "Metric for the n1-n3 link", metric);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4); // n0, n1, n2, n3

    // Create point-to-point links
    PointToPointHelper p2p;

    // n0 <-> n2 link
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer dev_n0n2 = p2p.Install(nodes.Get(0), nodes.Get(2));

    // n1 <-> n2 link
    NetDeviceContainer dev_n1n2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // n1 <-> n3 link (higher cost)
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(100)));
    NetDeviceContainer dev_n1n3 = p2p.Install(nodes.Get(1), nodes.Get(3));

    // n2 <-> n3 link (alternate path)
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer dev_n2n3 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n2 = address.Assign(dev_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n2 = address.Assign(dev_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n3 = address.Assign(dev_n1n3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n2n3 = address.Assign(dev_n2n3);

    // Set up static routing
    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4_n1 = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_n1 = routingHelper.GetStaticRouting(ipv4_n1);

    // Route from n1 to n3 via n2 with default metric, and direct link with configurable metric
    routing_n1->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"), 3, metric); // n1 -> n3 direct link
    routing_n1->AddHostRouteTo(Ipv4Address("10.1.4.2"), Ipv4Address("10.1.2.2"), 2, 1); // n1 -> n2

    Ptr<Ipv4> ipv4_n3 = nodes.Get(3)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> routing_n3 = routingHelper.GetStaticRouting(ipv4_n3);
    routing_n3->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.3.1"), 1, 1); // n3 -> n0 via n1

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("network-topology-simulation.tr"));
    p2p.EnablePcapAll("network-topology-simulation");

    // UDP traffic from n3 to n1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(if_n1n3.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}