#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixSimulation");

int main(int argc, char *argv[]) {
    LogComponentEnable("RadvdTwoPrefixSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0 = csma.Install(NodeContainer(nodes.Get(0), router.Get(0)));
    NetDeviceContainer devices1 = csma.Install(NodeContainer(router.Get(0), nodes.Get(1)));

    InternetStackHelper internet;
    internet.Install(nodes);
    internet.Install(router);

    Ipv6AddressHelper address;

    // Subnet 2001:1::/64 - n0 and R's interface
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = address.Assign(devices0);
    interfaces0.SetForwarding(1, true);
    interfaces0.SetDefaultRouteInAllNodes();

    // Assign second prefix on the same interface (R to n0)
    interfaces0.AddAddress(1, Ipv6Address("2001:ABCD::1"), Ipv6Prefix(64));

    // Subnet 2001:2::/64 - n1 and R's interface
    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = address.Assign(devices1);
    interfaces1.SetForwarding(0, true);
    interfaces1.SetDefaultRouteInAllNodes();

    // Enable RA for n0's subnet
    Ipv6InterfaceContainer radvdInterfaces;
    radvdInterfaces.Add(router.Get(0), interfaces0.Get(1).first);

    RadvdHelper radvdHelper;
    Ptr<Radvd> radvd = radvdHelper.Install(radvdInterfaces);

    // Create two prefixes for the first interface
    RadvdPrefixInformation prefix1;
    prefix1.SetPrefix(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    prefix1.SetOnLinkFlag(true);
    prefix1.SetAutonomousFlag(true);
    prefix1.SetPreferredLifetime(Seconds(600));
    prefix1.SetValidLifetime(Seconds(1800));

    RadvdPrefixInformation prefix2;
    prefix2.SetPrefix(Ipv6Address("2001:ABCD::"), Ipv6Prefix(64));
    prefix2.SetOnLinkFlag(true);
    prefix2.SetAutonomousFlag(true);
    prefix2.SetPreferredLifetime(Seconds(600));
    prefix2.SetValidLifetime(Seconds(1800));

    radvd->AddPrefix(prefix1);
    radvd->AddPrefix(prefix2);

    // Enable RA for n1's subnet
    Ipv6InterfaceContainer radvdInterfaces2;
    radvdInterfaces2.Add(router.Get(0), interfaces1.Get(0).first);

    Ptr<Radvd> radvd2 = radvdHelper.Install(radvdInterfaces2);

    RadvdPrefixInformation prefix3;
    prefix3.SetPrefix(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    prefix3.SetOnLinkFlag(true);
    prefix3.SetAutonomousFlag(true);
    prefix3.SetPreferredLifetime(Seconds(600));
    prefix3.SetValidLifetime(Seconds(1800));

    radvd2->AddPrefix(prefix3);

    // Ping application from n0 to n1
    V4PingHelper ping6(interfaces1.GetAddress(1, 1));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    Simulator::CreateDirectory("radvd-two-prefix.tr");
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("radvd-two-prefix.tr");
    csma.EnableAsciiAll(stream);

    NS_LOG_INFO("Starting simulation...");
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}