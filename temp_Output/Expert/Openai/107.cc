#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int main(int argc, char* argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer n0, n1, router;
    n0.Create(1);
    n1.Create(1);
    router.Create(1);

    // Segment 1: n0 <-> router
    NodeContainer net1;
    net1.Add(n0.Get(0));
    net1.Add(router.Get(0));

    // Segment 2: router <-> n1
    NodeContainer net2;
    net2.Add(router.Get(0));
    net2.Add(n1.Get(0));

    // CSMA helpers and NetDevices
    CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma1.SetDeviceAttribute("Mtu", UintegerValue(5000));

    CsmaHelper csma2;
    csma2.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma2.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer dev1 = csma1.Install(net1);
    NetDeviceContainer dev2 = csma2.Install(net2);

    // Install the IPv6 stack
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(n0);
    internetv6.Install(n1);
    internetv6.Install(router);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if1 = ipv6.Assign(dev1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if2 = ipv6.Assign(dev2);

    // Set all interfaces as up, except router's interface towards n1 (needs manual up)
    if1.SetForwarding(1, true);
    if1.SetDefaultRouteInAllNodes(1);
    if2.SetForwarding(0, true);
    if2.SetDefaultRouteInAllNodes(0);

    Ptr<Ipv6> ipv6Router = router.Get(0)->GetObject<Ipv6>();
    if (ipv6Router)
    {
        ipv6Router->SetForwarding(1, true);
        ipv6Router->SetDefaultRoute(1);
    }

    // Applications
    uint16_t port = 9;
    // UDP Echo Server on n1
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(n1.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on n0, target n1's IPv6 address
    UdpEchoClientHelper echoClient(if2.GetAddress(1,1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(4000)); // Large enough to trigger fragmentation
    ApplicationContainer clientApps = echoClient.Install(n0.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable global routing
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting(n0.Get(0)->GetObject<Ipv6>());
    staticRoutingN0->SetDefaultRoute(Ipv6Address("2001:1::2"), 1);

    Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting(n1.Get(0)->GetObject<Ipv6>());
    staticRoutingN1->SetDefaultRoute(Ipv6Address("2001:2::1"), 1);

    /// Enable tracing
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("fragmentation-ipv6-two-mtu.tr");
    csma1.EnableAsciiAll(stream);
    csma2.EnableAsciiAll(stream);
    csma1.EnablePcapAll("fragmentation-ipv6-two-mtu", true, true);
    csma2.EnablePcapAll("fragmentation-ipv6-two-mtu", true, true);

    // Trace receptions on sink
    Config::ConnectWithoutContext(
        "/NodeList/1/ApplicationList/0/$ns3::UdpEchoServer/Rx",
        MakeBoundCallback([](Ptr<const Packet> pkt, const Address &addr, const Address &from, uint16_t port, const Ptr<Application>& app) {
            std::ofstream outfile("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
            outfile << "Rx at server application: " << Simulator::Now().GetSeconds()
                    << "s, Packet size: " << pkt->GetSize() << std::endl;
        }, Ptr<Application>()));

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}