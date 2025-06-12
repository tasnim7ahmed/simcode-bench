#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-generator.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-helper.h"
#include "ns3/icmpv6-l4-protocol.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixSimulation");

void PrintIpv6Addresses(Ptr<Node> node, std::string name)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < ipv6->GetNAddresses(i); ++j)
        {
            Ipv6InterfaceAddress addr = ipv6->GetAddress(i, j);
            if (!addr.GetAddress().IsLinkLocal())
            {
                std::cout << name << " - Interface " << i
                          << " Address " << addr.GetAddress() << std::endl;
            }
        }
    }
}

void
RxTrace(std::string context, Ptr<const Packet> p, const Address &addr)
{
    static std::ofstream out("radvd-two-prefix.tr", std::ios_base::app);
    out << Simulator::Now().GetSeconds() << "s " << context << " Packet received, UID: " << p->GetUid() << " Size: " << p->GetSize() << std::endl;
}

void
QueueEnqueueTrace(std::string context, Ptr<const Packet> p)
{
    static std::ofstream out("radvd-two-prefix.tr", std::ios_base::app);
    out << Simulator::Now().GetSeconds() << "s " << context << " Enqueue UID: " << p->GetUid() << " Size: " << p->GetSize() << std::endl;
}

void
QueueDequeueTrace(std::string context, Ptr<const Packet> p)
{
    static std::ofstream out("radvd-two-prefix.tr", std::ios_base::app);
    out << Simulator::Now().GetSeconds() << "s " << context << " Dequeue UID: " << p->GetUid() << " Size: " << p->GetSize() << std::endl;
}

int main(int argc, char* argv[])
{
    LogComponentEnable("RadvdTwoPrefixSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2); // n0, n1
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1);

    Ptr<Node> router = CreateObject<Node>();

    // Two CSMA segments:
    // n0 <-> router, n1 <-> router
    NodeContainer n0r, n1r;
    n0r.Add(n0);
    n0r.Add(router);

    n1r.Add(n1);
    n1r.Add(router);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer d0r = csma.Install(n0r);
    NetDeviceContainer d1r = csma.Install(n1r);

    // Install Internet stack
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(n0);
    internetv6.Install(n1);
    internetv6.Install(router);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6_0;
    ipv6_0.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n0r = ipv6_0.Assign(d0r);
    ipv6_0.NewNetwork(); // advance to next network base

    // Second prefix for router-n0 link
    Ipv6AddressHelper ipv6_abcd;
    ipv6_abcd.SetBase(Ipv6Address("2001:ABCD::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n0r_abcd = ipv6_abcd.Assign(d0r);

    // Now router-n1 link
    Ipv6AddressHelper ipv6_1;
    ipv6_1.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n1r = ipv6_1.Assign(d1r);

    // Set interfaces up except router's loopback
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
        for (uint32_t j = 0; j < ipv6->GetNInterfaces(); ++j)
        {
            ipv6->SetUp(j);
        }
    }
    Ptr<Ipv6> ipv6Router = router->GetObject<Ipv6>();
    for (uint32_t j = 0; j < ipv6Router->GetNInterfaces(); ++j)
    {
        if (j != 0) // interface 0 is loopback
            ipv6Router->SetUp(j);
    }

    // Configure Router Advertisement Daemon (radvd) on router
    RadvdHelper radvdHelper;
    RadvdRouter radvdRouter;

    // Interface 1: router <-> n0 (d0r.Get(1))
    RadvdPrefixInformation pi0;
    pi0.SetPrefix(Ipv6Address("2001:1::"), 64);
    pi0.SetOnLinkFlag(true);
    pi0.SetAutonomousFlag(true);

    RadvdPrefixInformation pi1;
    pi1.SetPrefix(Ipv6Address("2001:ABCD::"), 64);
    pi1.SetOnLinkFlag(true);
    pi1.SetAutonomousFlag(true);

    RadvdInterface radvdIf0;
    radvdIf0.SetInterface(router->GetObject<NetDeviceContainer>()->Get(1)); // d0r.Get(1)
    radvdIf0.AddPrefix(pi0);
    radvdIf0.AddPrefix(pi1);
    radvdRouter.AddInterface(radvdIf0);

    // Interface 2: router <-> n1 (d1r.Get(1))
    RadvdPrefixInformation pi2;
    pi2.SetPrefix(Ipv6Address("2001:2::"), 64);
    pi2.SetOnLinkFlag(true);
    pi2.SetAutonomousFlag(true);
    RadvdInterface radvdIf1;
    radvdIf1.SetInterface(router->GetObject<NetDeviceContainer>()->Get(2)); // d1r.Get(1)
    radvdIf1.AddPrefix(pi2);
    radvdRouter.AddInterface(radvdIf1);

    ApplicationContainer radvdApps = radvdHelper.Install(router, radvdRouter);
    radvdApps.Start(Seconds(0.5));
    radvdApps.Stop(Seconds(15.0));

    // Ping application: n0 (src) -> n1 (dst)
    std::string targetPingAddr = if_n1r.GetAddress(0,1).GetAddress().ToString(); // Not link-local, so the global
    V6PingHelper ping6(Ipv6Address(targetPingAddr.c_str()));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Count", UintegerValue(5));
    ApplicationContainer apps = ping6.Install(n0);
    apps.Start(Seconds(4.0));
    apps.Stop(Seconds(14.0));

    // Tracing
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacTxQueue/Enqueue", MakeCallback(&QueueEnqueueTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacTxQueue/Dequeue", MakeCallback(&QueueDequeueTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/PhyRxEnd", MakeCallback(&RxTrace));

    // Print addresses
    Simulator::Schedule(Seconds(3.2), &PrintIpv6Addresses, n0, "n0");
    Simulator::Schedule(Seconds(3.2), &PrintIpv6Addresses, n1, "n1");
    Simulator::Schedule(Seconds(3.2), &PrintIpv6Addresses, router, "router");

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}