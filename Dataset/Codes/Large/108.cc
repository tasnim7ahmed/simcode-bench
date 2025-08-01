
// Network topology
//
//             STA2
//              |
//              |
//   R1         R2
//   |          |
//   |          |
//   ------------
//           |
//           |
//          STA 1
//
// - Initial configuration :
//         - STA1 default route : R1
//         - R1 static route to STA2 : R2
//         - STA2 default route : R2
// - STA1 send Echo Request to STA2 using its default route to R1
// - R1 receive Echo Request from STA1, and forward it to R2
// - R1 send an ICMPv6 Redirection to STA1 with Target STA2 and Destination R2
// - Next Echo Request from STA1 to STA2 are directly sent to R2

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectExample");

int
main(int argc, char** argv)
{
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("Icmpv6RedirectExample", LOG_LEVEL_INFO);
        LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
        LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("NdiscCache", LOG_LEVEL_ALL);
    }

    NS_LOG_INFO("Create nodes.");
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();
    NodeContainer net1(sta1, r1, r2);
    NodeContainer net2(r2, sta2);
    NodeContainer all(sta1, r1, r2, sta2);

    InternetStackHelper internetv6;
    internetv6.Install(all);

    NS_LOG_INFO("Create channels.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer ndc1 = csma.Install(net1);
    NetDeviceContainer ndc2 = csma.Install(net2);

    NS_LOG_INFO("Assign IPv6 Addresses.");
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    iic1.SetForwarding(2, true);
    iic1.SetForwarding(1, true);
    iic1.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic2 = ipv6.Assign(ndc2);
    iic2.SetForwarding(0, true);
    iic2.SetDefaultRouteInAllNodes(0);

    Ipv6StaticRoutingHelper routingHelper;

    // manually inject a static route to the second router.
    Ptr<Ipv6StaticRouting> routing = routingHelper.GetStaticRouting(r1->GetObject<Ipv6>());
    routing->AddHostRouteTo(iic2.GetAddress(1, 1),
                            iic1.GetAddress(2, 0),
                            iic1.GetInterfaceIndex(1));

    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(0.0), r1, routingStream);
    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(3.0), sta1, routingStream);

    NS_LOG_INFO("Create Applications.");
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    PingHelper ping(iic2.GetAddress(1, 1));

    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ApplicationContainer apps = ping.Install(sta1);
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("icmpv6-redirect.tr"));
    csma.EnablePcapAll("icmpv6-redirect", true);

    /* Now, do the actual simulation. */
    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
