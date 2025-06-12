#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create 3 nodes: A (0), B (1), C (2)
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between A <-> B and B <-> C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the links
    NetDeviceContainer devAtoB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBtoC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses with /32 subnet for each interface
    Ipv4AddressHelper ipv4AtoB;
    ipv4AtoB.SetBase("192.168.1.0", "255.255.255.255");  // /32
    Ipv4InterfaceContainer ipAtoB = ipv4AtoB.Assign(devAtoB);

    Ipv4AddressHelper ipv4BtoC;
    ipv4BtoC.SetBase("192.168.2.0", "255.255.255.255");  // /32
    Ipv4InterfaceContainer ipBtoC = ipv4BtoC.Assign(devBtoC);

    // Assign loopback addresses to nodes A, B, and C
    Ptr<LoopbackNetDevice> deviceA = CreateObject<LoopbackNetDevice>();
    nodes.Get(0)->AddDevice(deviceA);
    Ipv4InterfaceContainer loopbackA;
    loopbackA.Add(nodes.Get(0)->GetObject<Ipv4>()->Assign(loopbackA, Ipv4InterfaceContainer(), deviceA));
    loopbackA.SetAddress(0, Ipv4Address("1.1.1.1"), Ipv4Mask("/32"));

    Ptr<LoopbackNetDevice> deviceB = CreateObject<LoopbackNetDevice>();
    nodes.Get(1)->AddDevice(deviceB);
    Ipv4InterfaceContainer loopbackB;
    loopbackB.Add(nodes.Get(1)->GetObject<Ipv4>()->Assign(loopbackB, Ipv4InterfaceContainer(), deviceB));
    loopbackB.SetAddress(0, Ipv4Address("2.2.2.2"), Ipv4Mask("/32"));

    Ptr<LoopbackNetDevice> deviceC = CreateObject<LoopbackNetDevice>();
    nodes.Get(2)->AddDevice(deviceC);
    Ipv4InterfaceContainer loopbackC;
    loopbackC.Add(nodes.Get(2)->GetObject<Ipv4>()->Assign(loopbackC, Ipv4InterfaceContainer(), deviceC));
    loopbackC.SetAddress(0, Ipv4Address("3.3.3.3"), Ipv4Mask("/32"));

    // Configure static routes using Ipv4StaticRoutingHelper
    Ipv4StaticRoutingHelper routingHelper;

    // Route from node A to node C via node B
    Ptr<Ipv4StaticRouting> routeA = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    routeA->AddHostRouteTo(Ipv4Address("3.3.3.3"), Ipv4Address("192.168.1.2"), 1);  // via AtoB link

    // Route from node C to node A via node B
    Ptr<Ipv4StaticRouting> routeC = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    routeC->AddHostRouteTo(Ipv4Address("1.1.1.1"), Ipv4Address("192.168.2.1"), 1);  // via BtoC link

    // Bi-directional routes on node B
    Ptr<Ipv4StaticRouting> routeB = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    routeB->AddHostRouteTo(Ipv4Address("1.1.1.1"), Ipv4Address("192.168.1.1"), 0);  // via AtoB
    routeB->AddHostRouteTo(Ipv4Address("3.3.3.3"), Ipv4Address("192.168.2.2"), 1);  // via BtoC

    // Setup OnOff application from node A to node C
    uint16_t port = 9;  // UDP port
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("3.3.3.3"), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer appOnOff = onoff.Install(nodes.Get(0));
    appOnOff.Start(Seconds(1.0));
    appOnOff.Stop(Seconds(10.0));

    // Setup PacketSink on node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("3.3.3.3"), port));
    ApplicationContainer appSink = sink.Install(nodes.Get(2));
    appSink.Start(Seconds(0.0));
    appSink.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("router_network");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}