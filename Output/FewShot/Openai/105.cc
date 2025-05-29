#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer endNodes;
    endNodes.Create(2); // n0 and n1
    NodeContainer router;
    router.Create(1);   // r

    // n0--r
    NodeContainer n0r;
    n0r.Add(endNodes.Get(0));
    n0r.Add(router.Get(0));
    // n1--r
    NodeContainer n1r;
    n1r.Add(endNodes.Get(1));
    n1r.Add(router.Get(0));

    // CSMA configurations
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Connecting n0--r and n1--r via separate CSMA segments
    NetDeviceContainer d_n0r = csma.Install(n0r);
    NetDeviceContainer d_n1r = csma.Install(n1r);

    // Internet Stack
    InternetStackHelper internetv6;
    internetv6.Install(endNodes);
    internetv6.Install(router);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n0r = ipv6.Assign(d_n0r);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n1r = ipv6.Assign(d_n1r);

    // Enable forwarding on the router
    Ptr<Ipv6> ipv6r = router.Get(0)->GetObject<Ipv6>();
    ipv6r->SetAttribute("IpForward", BooleanValue(true));

    // Set default routes on n0 and n1
    Ptr<Ipv6StaticRouting> n0StaticRouting = Ipv6RoutingHelper::GetStaticRouting(endNodes.Get(0)->GetObject<Ipv6>());
    n0StaticRouting->SetDefaultRoute(if_n0r.GetAddress(1, 1), 1);

    Ptr<Ipv6StaticRouting> n1StaticRouting = Ipv6RoutingHelper::GetStaticRouting(endNodes.Get(1)->GetObject<Ipv6>());
    n1StaticRouting->SetDefaultRoute(if_n1r.GetAddress(1, 1), 1);

    // Set up UDP echo server on n1
    uint16_t port = 9999;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(endNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(12.0));

    // Set up UDP echo client on n0; send large payload to force fragmentation (e.g., 4000 bytes)
    UdpEchoClientHelper echoClient(if_n1r.GetAddress(0, 1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(4000));

    ApplicationContainer clientApps = echoClient.Install(endNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(12.0));

    // Trace queue and packet reception events
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6.tr"));

    // Enable pcap tracing on all CSMA interfaces
    csma.EnablePcapAll("fragmentation-ipv6", true);

    Simulator::Stop(Seconds(13.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}