#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixSim");

std::ofstream traceFile;

void
QueueTxCallback(std::string context, Ptr<const Packet> packet)
{
    traceFile << Simulator::Now().GetSeconds() << " " << context << " TX " << packet->GetSize() << " bytes\n";
}

void
QueueRxCallback(std::string context, Ptr<const Packet> packet)
{
    traceFile << Simulator::Now().GetSeconds() << " " << context << " RX " << packet->GetSize() << " bytes\n";
}

void
PacketReceivedCallback(Ptr<const Packet> packet, const Address &address)
{
    traceFile << Simulator::Now().GetSeconds() << " Packet received: " << packet->GetSize() << " bytes\n";
}

void
PrintIpv6Addresses(NodeContainer nodes, std::string label)
{
    std::cout << "--- IPv6 Addresses (" << label << ") ---" << std::endl;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        std::cout << "Node " << node->GetId() << std::endl;
        for (uint32_t iface = 0; iface < ipv6->GetNInterfaces(); ++iface)
        {
            for (uint32_t addr = 0; addr < ipv6->GetNAddresses(iface); ++addr)
            {
                std::cout << "  Interface " << iface << " Address " << addr << ": " << ipv6->GetAddress(iface, addr) << std::endl;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    LogComponentEnable("RadvdTwoPrefixSim", LOG_LEVEL_INFO);

    // Open trace file
    traceFile.open("radvd-two-prefix.tr");
    if (!traceFile.is_open())
    {
        NS_LOG_ERROR("Failed to open trace file.");
        return 1;
    }

    // Create nodes
    NodeContainer n0, n1, router;
    n0.Create(1);
    n1.Create(1);
    router.Create(1);

    // CSMA channels
    NodeContainer net1(n0, router); // n0 <-> router
    NodeContainer net2(router, n1); // router <-> n1

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer dNet1 = csma.Install(net1);
    NetDeviceContainer dNet2 = csma.Install(net2);

    // Internet stack
    InternetStackHelper internetv6;
    internetv6.Install(n0);
    internetv6.Install(router);
    internetv6.Install(n1);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6Net1;
    ipv6Net1.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iNet1 = ipv6Net1.Assign(dNet1);
    iNet1.SetRouter(1, true); // set router for router interface

    // Assign second prefix to router's interface towards n0
    Ipv6InterfaceAddress ifAddrRouter2 = Ipv6InterfaceAddress(Ipv6Address("2001:ABCD::1"), Ipv6Prefix(64));
    Ptr<Ipv6> routerIpv6 = router.Get(0)->GetObject<Ipv6>();
    uint32_t routerIfNet1 = routerIpv6->GetInterfaceForDevice(dNet1.Get(1));
    routerIpv6->AddAddress(routerIfNet1, ifAddrRouter2);

    Ipv6AddressHelper ipv6Net2;
    ipv6Net2.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iNet2 = ipv6Net2.Assign(dNet2);
    iNet2.SetRouter(0, true); // set router for router interface

    // By default, NS-3 sets all interfaces to down. Bring them up.
    for (uint32_t i = 0; i < n0.Get(0)->GetObject<Ipv6>()->GetNInterfaces(); ++i)
        n0.Get(0)->GetObject<Ipv6>()->SetUp(i);
    for (uint32_t i = 0; i < n1.Get(0)->GetObject<Ipv6>()->GetNInterfaces(); ++i)
        n1.Get(0)->GetObject<Ipv6>()->SetUp(i);
    for (uint32_t i = 0; i < router.Get(0)->GetObject<Ipv6>()->GetNInterfaces(); ++i)
        router.Get(0)->GetObject<Ipv6>()->SetUp(i);

    // RAdvD configuration for net1: two prefixes
    RadvdHelper radvdHelper;
    std::vector<RadvdHelper::RadvdInterfaceConfig> radvdIfs;

    // Net1 interface (router to n0): two prefixes
    RadvdHelper::RadvdInterfaceConfig ifConf1;
    ifConf1.interface = routerIfNet1;
    RadvdPrefixInformation pi1;
    pi1.prefix = Ipv6Address("2001:1::");
    pi1.prefixLength = 64;
    pi1.onLink = true;
    pi1.autonomous = true;
    RadvdPrefixInformation pi2;
    pi2.prefix = Ipv6Address("2001:ABCD::");
    pi2.prefixLength = 64;
    pi2.onLink = true;
    pi2.autonomous = true;
    ifConf1.prefixes.push_back(pi1);
    ifConf1.prefixes.push_back(pi2);
    radvdIfs.push_back(ifConf1);

    // Net2 interface (router to n1): one prefix
    uint32_t routerIfNet2 = router.Get(0)->GetObject<Ipv6>()->GetInterfaceForDevice(dNet2.Get(0));
    RadvdHelper::RadvdInterfaceConfig ifConf2;
    ifConf2.interface = routerIfNet2;
    RadvdPrefixInformation pi3;
    pi3.prefix = Ipv6Address("2001:2::");
    pi3.prefixLength = 64;
    pi3.onLink = true;
    pi3.autonomous = true;
    ifConf2.prefixes.push_back(pi3);
    radvdIfs.push_back(ifConf2);

    radvdHelper.SetInterfaces(radvdIfs);
    radvdHelper.Install(router.Get(0));

    // Schedule IP printing
    Simulator::Schedule(Seconds(1.5), &PrintIpv6Addresses, n0, "n0");
    Simulator::Schedule(Seconds(1.5), &PrintIpv6Addresses, router, "router");
    Simulator::Schedule(Seconds(1.5), &PrintIpv6Addresses, n1, "n1");

    // Wait a bit to ensure addresses assigned by SLAAC
    double appStartTime = 2.0;

    // Find n1's global address on net2
    Ptr<Ipv6> ipv6n1 = n1.Get(0)->GetObject<Ipv6>();
    int n1if = 1; // Only one interface
    Ipv6Address n1addr;
    for (uint32_t j = 0; j < ipv6n1->GetNAddresses(n1if); ++j)
    {
        Ipv6Address addr = ipv6n1->GetAddress(n1if, j).GetAddress();
        if (!addr.IsLinkLocal() && !addr.IsLoopback())
        {
            n1addr = addr;
            break;
        }
    }

    // Install V6 Ping application: n0 pings n1
    V6PingHelper pingHelper(n1addr);
    pingHelper.SetAttribute("StartTime", TimeValue(Seconds(appStartTime)));
    pingHelper.SetAttribute("StopTime", TimeValue(Seconds(appStartTime + 6)));
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pingApps = pingHelper.Install(n0.Get(0));

    // Trace queues on both CSMA interfaces
    Ptr<NetDeviceQueueInterface> qInterface1 = dNet1.Get(0)->GetQueueInterface();
    Ptr<NetDeviceQueueInterface> qInterface2 = dNet1.Get(1)->GetQueueInterface();
    Ptr<NetDeviceQueueInterface> qInterface3 = dNet2.Get(0)->GetQueueInterface();
    Ptr<NetDeviceQueueInterface> qInterface4 = dNet2.Get(1)->GetQueueInterface();

    // Attach tracing for TX and RX on queues
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/TxQueue/Enqueue", MakeBoundCallback(&QueueTxCallback, "ENQUEUE"));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/TxQueue/Dequeue", MakeBoundCallback(&QueueRxCallback, "DEQUEUE"));

    // Packet reception on n1
    Ptr<NetDevice> n1dev = dNet2.Get(1);
    n1dev->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&PacketReceivedCallback));

    // Simulation
    Simulator::Stop(Seconds(appStartTime + 8));
    Simulator::Run();
    Simulator::Destroy();

    traceFile.close();

    return 0;
}