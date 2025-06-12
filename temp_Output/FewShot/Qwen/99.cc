#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);

    // Create three nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point helper for links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create devices and channels between A-B and B-C
    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    // Link A-B
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipAB = ipv4.Assign(devAB);

    // Link B-C
    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ipBC = ipv4.Assign(devBC);

    // Assign /32 host routes to node A and node C
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrA = Ipv4InterfaceAddress(Ipv4Address("10.0.0.1"), Ipv4Mask::Get32());
    ipv4A->AddAddress(0, ifAddrA);
    ipv4A->SetMetric(0, 1);

    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrC = Ipv4InterfaceAddress(Ipv4Address("10.0.0.2"), Ipv4Mask::Get32());
    ipv4C->AddAddress(0, ifAddrC);
    ipv4C->SetMetric(0, 1);

    // Configure static routing for hosts

    // Node A routing (default route via B)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> aRouting = ipv4RoutingHelper.GetStaticRouting(ipv4A);
    aRouting->AddNetworkRouteTo(Ipv4Address("10.0.0.2"), Ipv4Mask::Get32(), ipAB.GetAddress(1), 1);

    // Node C routing (default route via B)
    Ptr<Ipv4StaticRouting> cRouting = ipv4RoutingHelper.GetStaticRouting(ipv4C);
    cRouting->AddNetworkRouteTo(Ipv4Address("10.0.0.1"), Ipv4Mask::Get32(), ipBC.GetAddress(0), 1);

    // Inject routes at node B using global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP OnOff application from A to C
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ifAddrC.GetLocal(), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Sink application on node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    app = sink.Install(nodes.Get(2));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Enable PCAP tracing for all devices
    p2p.EnablePcapAll("router-network");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}