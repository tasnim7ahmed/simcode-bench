#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);

    // Create nodes: n0, r (router), and n1
    NodeContainer nodes;
    nodes.Create(3);

    // CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect n0 to router
    NetDeviceContainer devN0R;
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r = nodes.Get(1);
    NodeContainer n0r(n0, r);
    devN0R = csma.Install(n0r);

    // Connect router to n1
    NetDeviceContainer devRN1;
    Ptr<Node> n1 = nodes.Get(2);
    NodeContainer rn1(r, n1);
    devRN1 = csma.Install(rn1);

    // Install IPv6 stack on all nodes
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.SetIpv6StackInstall(true);
    internetv6.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    // n0 - router link
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iicN0R = ipv6.Assign(devN0R);
    iicN0R.SetForwarding(1, true);  // enable forwarding on router

    // router - n1 link
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iicRN1 = ipv6.Assign(devRN1);
    iicRN1.SetForwarding(0, true);  // enable forwarding on router

    // Set up default routes for n0 and n1 via the router
    Ipv6StaticRoutingHelper routingHelper;

    // Route from n0 to n1 through router
    Ptr<Ipv6StaticRouting> n0routing = routingHelper.GetStaticRouting(n0->GetObject<Ipv6>());
    n0routing->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 1);  // next hop interface index

    // Route from n1 to n0 through router
    Ptr<Ipv6StaticRouting> n1routing = routingHelper.GetStaticRouting(n1->GetObject<Ipv6>());
    n1routing->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);

    // Setup UDP Echo Server on n1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(n1);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP Echo Client on n0 with large packet size to induce fragmentation
    UdpEchoClientHelper echoClient(iicRN1.GetAddress(1, 1), port);  // n1's address
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1500));  // Large enough to fragment

    ApplicationContainer clientApp = echoClient.Install(n0);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Open trace file for queue and packet reception events
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("fragmentation-ipv6.tr");

    // Enable tracing of queues and receptions
    csma.EnablePcapAll("fragmentation-ipv6", false);
    csma.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}