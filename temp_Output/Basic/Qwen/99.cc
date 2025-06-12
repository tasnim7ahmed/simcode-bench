#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterGlobalRouting");

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

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    interfacesBC = address.Assign(devicesBC);

    Ipv4Address nodeAHostAddr = interfacesAB.GetAddress(0);
    Ipv4Address nodeCHostAddr = interfacesBC.GetAddress(1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Inject host routes at node B (index 1)
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4RoutingProtocol> routingB = ipv4B->GetRoutingProtocol();
    Ptr<Ipv4StaticRouting> staticRoutingB = DynamicCast<Ipv4StaticRouting>(routingB);
    if (staticRoutingB) {
        staticRoutingB->AddHostRouteTo(nodeCHostAddr, interfacesBC.GetAddress(0), 2); // route to node C via interface towards C
        staticRoutingB->AddHostRouteTo(nodeAHostAddr, interfacesAB.GetAddress(1), 1); // route to node A via interface towards A
    }

    // OnOff UDP Application from node A to node C
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(nodeCHostAddr, port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Sink application at node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    app = sink.Install(nodes.Get(2));
    app.Start(Seconds(0.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("three-router-global-routing");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}