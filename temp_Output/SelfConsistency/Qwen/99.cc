#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterGlobalRoutingExample");

int main(int argc, char *argv[]) {
    // Log components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);  // A (0), B (1), C (2)

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Links: A <-> B and B <-> C
    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack with global routing
    InternetStackHelper stack;
    Ipv4GlobalRoutingHelper globalRouting;
    stack.SetRoutingHelper(globalRouting); // Use global routing protocol
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = address.Assign(devAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBC = address.Assign(devBC);

    // Manually add host routes at node B
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4RoutingTableEntry> routeA = Ipv4RoutingTableEntry::CreateHostRouteTo(
        ifAB.GetAddress(0),  // IP of node A's interface
        1);                  // Output interface index (to node A)
    ipv4B->GetRoutingProtocol()->NotifyAddRoute(routeA);

    Ptr<Ipv4RoutingTableEntry> routeC = Ipv4RoutingTableEntry::CreateHostRouteTo(
        ifBC.GetAddress(1),  // IP of node C's interface
        2);                  // Output interface index (to node C)
    ipv4B->GetRoutingProtocol()->NotifyAddRoute(routeC);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // OnOff UDP application from A to C
    uint16_t port = 9; // Discard port

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ifBC.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps = onoff.Install(nodes.Get(0)); // From node A
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Sink application at node C
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(nodes.Get(2)); // At node C
    apps.Start(Seconds(0.0));

    // Enable packet tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-global-routing.tr"));
    p2p.EnablePcapAll("three-router-global-routing");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}