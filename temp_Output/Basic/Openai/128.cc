#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-ospf-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfLinkStateRoutingExample");

int
main(int argc, char *argv[])
{
    LogComponentEnable("OspfLinkStateRoutingExample", LOG_LEVEL_INFO);
    double simTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect routers in a square: 0-1-2-3-0, plus 1-3 for redundant path
    NetDeviceContainer d0d1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer d1d2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer d2d3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer d3d0 = p2p.Install(routers.Get(3), routers.Get(0));
    NetDeviceContainer d1d3 = p2p.Install(routers.Get(1), routers.Get(3));

    // Install Internet Stack with OSPF enabled
    Ipv4OspfHelper ospfRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospfRouting, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(routers);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if0_1 = address.Assign(d0d1);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if1_2 = address.Assign(d1d2);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if2_3 = address.Assign(d2d3);

    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if3_0 = address.Assign(d3d0);

    address.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer if1_3 = address.Assign(d1d3);

    // Enable Pcap tracing to capture OSPF LSA exchanges
    p2p.EnablePcapAll("ospf-lsa");

    // Install UDP traffic generator from router 0 to router 2
    uint16_t port = 4000;
    OnOffHelper clientHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(if1_2.GetAddress(1), port)));
    clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simTime-2)));

    ApplicationContainer clientApps = clientHelper.Install(routers.Get(0));

    // Install UDP packet sink on router 2
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer sinkApps = sinkHelper.Install(routers.Get(2));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simTime));

    // Simulate a link failure event to trigger LSA propagation:
    // Link down between routers 1 and 3 at t = 8.0s
    Simulator::Schedule(Seconds(8.0), [&] {
        d1d3.Get(0)->SetDown();
        d1d3.Get(1)->SetDown();
        NS_LOG_INFO("Link between Router 1 and 3 is DOWN");
    });

    // Restore the link at t = 14.0s
    Simulator::Schedule(Seconds(14.0), [&] {
        d1d3.Get(0)->SetUp();
        d1d3.Get(1)->SetUp();
        NS_LOG_INFO("Link between Router 1 and 3 is UP");
    });

    // Optional: NetAnim visualization
    AnimationInterface anim("ospf_lsa_anim.xml");
    anim.SetConstantPosition(routers.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(routers.Get(1), 150.0, 50.0);
    anim.SetConstantPosition(routers.Get(2), 150.0, 150.0);
    anim.SetConstantPosition(routers.Get(3), 50.0, 150.0);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}