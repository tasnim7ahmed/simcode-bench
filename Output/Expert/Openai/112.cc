#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/icmpv6-echo-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/radvd-helper.h"
#include "ns3/trace-helper.h"
#include <fstream>

using namespace ns3;

void PrintIpv6Addresses(Ptr<Node> node, uint32_t interfaceIndex)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    std::cout << "Node " << node->GetId() << " Interface " << interfaceIndex << " addresses:\n";
    for (uint32_t a = 0; a < ipv6->GetNAddresses(interfaceIndex); ++a)
    {
        std::cout << "  " << ipv6->GetAddress(interfaceIndex, a).GetAddress() << "/" << int(ipv6->GetAddress(interfaceIndex, a).GetPrefixLength()) << std::endl;
    }
}

void RxTrace(std::string context, Ptr<const Packet> packet, Ipv6Address addr, uint16_t port)
{
    static std::ofstream ofs("radvd-two-prefix.tr", std::ios::app);
    ofs << Simulator::Now().GetSeconds() << " " << context << " RX " << packet->GetSize() << " bytes\n";
}

void QueueTrace(std::string context, uint32_t oldValue, uint32_t newValue)
{
    static std::ofstream ofs("radvd-two-prefix.tr", std::ios::app);
    ofs << Simulator::Now().GetSeconds() << " " << context << " QUEUE " << oldValue << "->" << newValue << "\n";
}

int main(int argc, char *argv[])
{
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6Echo", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2); // n0, n1
    NodeContainer router;
    router.Create(1); // R

    NodeContainer n0r;
    n0r.Add(nodes.Get(0));
    n0r.Add(router.Get(0));

    NodeContainer n1r;
    n1r.Add(nodes.Get(1));
    n1r.Add(router.Get(0));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer d_n0r = csma.Install(n0r);
    NetDeviceContainer d_n1r = csma.Install(n1r);

    InternetStackHelper stack;
    stack.Install(nodes);
    stack.Install(router);

    Ipv6AddressHelper ipv6_0;
    ipv6_0.SetBase("2001:1::", 64);
    Ipv6InterfaceContainer if_n0r = ipv6_0.Assign(d_n0r);

    Ipv6AddressHelper ipv6_1;
    ipv6_1.SetBase("2001:2::", 64);
    Ipv6InterfaceContainer if_n1r = ipv6_1.Assign(d_n1r);

    // Assign a second prefix to R's interface towards n0
    Ptr<Ipv6> ipv6R = router.Get(0)->GetObject<Ipv6>();
    uint32_t rIf0 = 0; // interface to n0
    uint32_t rIf1 = 1; // interface to n1
    ipv6R->AddAddress(rIf0, Ipv6InterfaceAddress(Ipv6Address("2001:ABCD::1"), Ipv6Prefix(64)));
    ipv6R->SetUp(rIf0);
    // Set up all interfaces
    ipv6R->SetUp(rIf1);
    nodes.Get(0)->GetObject<Ipv6>()->SetUp(1);
    nodes.Get(1)->GetObject<Ipv6>()->SetUp(1);

    // Enable global routing
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    RadvdHelper radvd;

    // Configure RAs to n0's subnet (two prefixes)
    RadvdInterface radvdi;
    radvdi.SetInterface(rIf0);
    radvdi.AddPrefix("2001:1::", 64, true, true, Seconds(1800), Seconds(1600));
    radvdi.AddPrefix("2001:ABCD::", 64, true, true, Seconds(1800), Seconds(1600));
    radvdi.SetSendAdvert(true);
    radvdi.SetManagedFlag(false);
    radvdi.SetOtherConfigFlag(false);
    radvd.AddRadvdInterface(router.Get(0), radvdi);

    // RAs to n1's subnet
    RadvdInterface radvdi1;
    radvdi1.SetInterface(rIf1);
    radvdi1.AddPrefix("2001:2::", 64, true, true, Seconds(1800), Seconds(1600));
    radvdi1.SetSendAdvert(true);
    radvdi1.SetManagedFlag(false);
    radvdi1.SetOtherConfigFlag(false);
    radvd.AddRadvdInterface(router.Get(0), radvdi1);

    ApplicationContainer radvdApps = radvd.Install(router.Get(0));
    radvdApps.Start(Seconds(1.0));

    // Tracing
    d_n0r.Get(0)->TraceConnect("MacTxQueueSize", "n0r/TxQueue", MakeCallback(&QueueTrace));
    d_n0r.Get(1)->TraceConnect("MacTxQueueSize", "n0r/TxQueue", MakeCallback(&QueueTrace));
    d_n1r.Get(0)->TraceConnect("MacTxQueueSize", "n1r/TxQueue", MakeCallback(&QueueTrace));
    d_n1r.Get(1)->TraceConnect("MacTxQueueSize", "n1r/TxQueue", MakeCallback(&QueueTrace));
    Ptr<Ipv6> ipv6_n1 = nodes.Get(1)->GetObject<Ipv6>();
    ipv6_n1->GetObject<Ipv6L3Protocol>()->TraceConnect("Rx", "n1/IPv6Rx", MakeCallback(&RxTrace));

    // Print IPv6 addresses at startup
    Simulator::Schedule(Seconds(2.0), &PrintIpv6Addresses, nodes.Get(0), 1);
    Simulator::Schedule(Seconds(2.0), &PrintIpv6Addresses, router.Get(0), 0);
    Simulator::Schedule(Seconds(2.0), &PrintIpv6Addresses, router.Get(0), 1);
    Simulator::Schedule(Seconds(2.0), &PrintIpv6Addresses, nodes.Get(1), 1);

    // Ping6 from n0 to n1
    Ipv6Address dst = if_n1r.GetAddress(0,1); // global address of n1
    Icmpv6EchoHelper pingHelper(dst);
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Verbose", BooleanValue(true));
    pingHelper.SetAttribute("MaxPackets", UintegerValue(5));
    ApplicationContainer pingApps = pingHelper.Install(nodes.Get(0));
    pingApps.Start(Seconds(5.0));
    pingApps.Stop(Seconds(15.0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}