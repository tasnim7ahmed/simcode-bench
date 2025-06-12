#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterGlobalRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack with global routing on all nodes
    InternetStackHelper stack;
    stack.SetRoutingHelper(Ipv4GlobalRoutingHelper()); // enables global routing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBC = ipv4.Assign(devBC);

    // Assign /32 host routes to node A and node C
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrA = Ipv4InterfaceAddress("192.168.1.1", "255.255.255.255");
    uint32_t idxA = 1; // secondary interface index
    ipv4A->AddAddress(0, ifAddrA); // assuming device 0 is correct

    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ipv4InterfaceAddress ifAddrC = Ipv4InterfaceAddress("192.168.2.1", "255.255.255.255");
    uint32_t idxC = 1; // secondary interface index
    ipv4C->AddAddress(0, ifAddrC); // assuming device 0 is correct

    // Inject static routes at node B (node index 1)
    Ipv4StaticRoutingHelper routeHelper;
    Ptr<Ipv4StaticRouting> ipv4StaticRoutingB = routeHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    NS_ASSERT(ipv4StaticRoutingB);

    // Route from A's /32 to C via B
    ipv4StaticRoutingB->AddHostRouteTo("192.168.1.1", "10.1.1.1", 1); // next hop is A side of AB link
    ipv4StaticRoutingB->AddHostRouteTo("192.168.2.1", "10.1.2.2", 2); // next hop is C side of BC link

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP OnOff application from node A to node C
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ifAddrC.GetLocal(), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps = onoff.Install(nodes.Get(0)); // node A
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Sink application at node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(nodes.Get(2)); // node C
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    // Enable packet tracing and PCAP capture
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router-routing.tr"));
    p2p.EnablePcapAll("three-router-routing");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}