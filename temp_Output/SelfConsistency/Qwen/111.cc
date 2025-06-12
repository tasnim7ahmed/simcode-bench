#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/radvd-interface.h"
#include "ns3/radvd-prefix.h"
#include "ns3/radvd.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdSimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RadvdSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
        LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
    }

    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);
    NodeContainer all(n0, r, n1);

    // Create and configure CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer ndc1 = csma.Install(net1);
    NetDeviceContainer ndc2 = csma.Install(net2);

    // Install IPv6 stack
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(all);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    iic1.SetForwarding(1, true);  // Enable forwarding on router for interface 1

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic2 = ipv6.Assign(ndc2);
    iic2.SetForwarding(0, true);  // Enable forwarding on router for interface 2

    // Set default routers
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> rtrRouting = routingHelper.GetStaticRouting(iic1.Get(1)->GetNode()->GetObject<Ipv6>());
    rtrRouting->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);
    rtrRouting->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2);

    // Configure Radvd on the router
    RadvdHelper radvdHelper;

    // Interface 0 (towards n0)
    radvdHelper.AddAnnouncedInterface(r, 0);
    radvdHelper.AddDefaultRouterPrefix(0, Ipv6Address("2001:1::"), 64);

    // Interface 1 (towards n1)
    radvdHelper.AddAnnouncedInterface(r, 1);
    radvdHelper.AddDefaultRouterPrefix(1, Ipv6Address("2001:2::"), 64);

    ApplicationContainer radvdApp = radvdHelper.Install(r);
    radvdApp.Start(Seconds(1.0));
    radvdApp.Stop(Seconds(10.0));

    // Ping application from n0 to n1
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping;
    ping.SetLocal(n0->GetObject<Ipv6>());
    ping.SetRemote(Ipv6Address("2001:2::200")); // Address of n1
    ping.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ping.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer app = ping.Install(n0);
    app.Start(Seconds(2.0));
    app.Stop(Seconds(7.0));

    // Tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("radvd.tr"));
    csma.EnablePcapAll("radvd");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}