#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address-generator.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixExample");

// Tracing callback for packet receptions
void RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream rxFile("radvd-two-prefix.tr", std::ios_base::app);
    rxFile << Simulator::Now().GetSeconds() << " Packet received at " << context << ", PacketSize: " << packet->GetSize() << std::endl;
}

// Tracing callback for queue events
void QueueTrace(std::string context, Ptr<const QueueBase> queue)
{
    static std::ofstream queueFile("radvd-two-prefix.tr", std::ios_base::app);
    queueFile << Simulator::Now().GetSeconds() << " QueueSize at " << context << " is " << queue->GetNPackets() << std::endl;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("RadvdApplication", LOG_LEVEL_INFO);
    LogComponentEnable("V4Ping", LOG_LEVEL_INFO);
    LogComponentEnable("V6Ping", LOG_LEVEL_INFO);

    NodeContainer hosts;
    hosts.Create(2);
    Ptr<Node> n0 = hosts.Get(0);
    Ptr<Node> n1 = hosts.Get(1);

    Ptr<Node> router = CreateObject<Node>();
    Names::Add("Router", router);
    Names::Add("Node0", n0);
    Names::Add("Node1", n1);

    // Segment between n0 and R
    NodeContainer net1(n0, router);
    CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma1.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer devs1 = csma1.Install(net1);

    // Segment between n1 and R
    NodeContainer net2(n1, router);
    CsmaHelper csma2;
    csma2.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma2.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer devs2 = csma2.Install(net2);

    // Install IPv6 Internet stack
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(hosts);
    internetv6.Install(router);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6_1;
    ipv6_1.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifs1 = ipv6_1.Assign(devs1);

    // Assign secondary prefix to router's interface on n0 subnet
    Ptr<NetDevice> r_dev1 = devs1.Get(1);
    Ptr<Ipv6> r_ipv6 = router->GetObject<Ipv6>();
    uint32_t ifIndexR1 = r_ipv6->GetInterfaceForDevice(r_dev1);
    r_ipv6->AddAddress(ifIndexR1, Ipv6InterfaceAddress(Ipv6Address("2001:ABCD::1"), Ipv6Prefix(64)));
    r_ipv6->SetUp(ifIndexR1);

    // Assign IPv6 on n1-R segment
    Ipv6AddressHelper ipv6_2;
    ipv6_2.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifs2 = ipv6_2.Assign(devs2);

    // Set interfaces up and forward
    for (uint32_t i = 0; i < n0->GetNDevices(); ++i)
    {
        Ptr<NetDevice> dev = n0->GetDevice(i);
        Ptr<Ipv6> ipv6 = n0->GetObject<Ipv6>();
        uint32_t ifIndex = ipv6->GetInterfaceForDevice(dev);
        if (ifIndex != -1u) {
            ipv6->SetUp(ifIndex);
        }
    }
    for (uint32_t i = 0; i < n1->GetNDevices(); ++i)
    {
        Ptr<NetDevice> dev = n1->GetDevice(i);
        Ptr<Ipv6> ipv6 = n1->GetObject<Ipv6>();
        uint32_t ifIndex = ipv6->GetInterfaceForDevice(dev);
        if (ifIndex != -1u) {
            ipv6->SetUp(ifIndex);
        }
    }
    for (uint32_t i = 0; i < router->GetNDevices(); ++i)
    {
        Ptr<NetDevice> dev = router->GetDevice(i);
        Ptr<Ipv6> ipv6 = router->GetObject<Ipv6>();
        uint32_t ifIndex = ipv6->GetInterfaceForDevice(dev);
        if (ifIndex != -1u) {
            ipv6->SetUp(ifIndex);
            ipv6->SetForwarding(ifIndex, true);
        }
    }

    // Configure radvd (Router Advertisement Daemon)
    RadvdHelper radvdHelper;
    RadvdInterface radvdIface1;
    radvdIface1.SetInterface(devs1.Get(1)); // R's interface to n0
    radvdIface1.AddPrefix(Ipv6Address("2001:1::"), 64, RadvdPrefix::ADVERTISABLE);
    radvdIface1.AddPrefix(Ipv6Address("2001:ABCD::"), 64, RadvdPrefix::ADVERTISABLE);
    radvdIface1.SetSendAdvert(true);

    RadvdInterface radvdIface2;
    radvdIface2.SetInterface(devs2.Get(1)); // R's interface to n1
    radvdIface2.AddPrefix(Ipv6Address("2001:2::"), 64, RadvdPrefix::ADVERTISABLE);
    radvdIface2.SetSendAdvert(true);

    std::vector<RadvdInterface> radvdIfaces;
    radvdIfaces.push_back(radvdIface1);
    radvdIfaces.push_back(radvdIface2);

    ApplicationContainer radvdApps = radvdHelper.Install(router, radvdIfaces);
    radvdApps.Start(Seconds(1.0));
    radvdApps.Stop(Seconds(29.0));

    // Wait for address/config discovery
    Simulator::Schedule(Seconds(3.0), [&]() {
        NS_LOG_UNCOND("At " << Simulator::Now().GetSeconds() << "s");
        for (uint32_t i = 0; i < hosts.GetN(); ++i) {
            Ptr<Ipv6> ipv6 = hosts.Get(i)->GetObject<Ipv6>();
            NS_LOG_UNCOND("Host " << i << " has addresses:");
            for (uint32_t j = 0; j < ipv6->GetNInterfaces(); ++j) {
                for (uint32_t k = 0; k < ipv6->GetNAddresses(j); ++k) {
                    NS_LOG_UNCOND("  " << ipv6->GetAddress(j, k).GetAddress());
                }
            }
        }
    });

    // Tracing - queues
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/TxQueue/PacketsInQueue", MakeCallback(&QueueTrace));
    // Tracing - packet reception
    Config::Connect("/NodeList/*/DeviceList/*/MacRx", MakeCallback(&RxTrace));

    // Configure Ping6 from n0 to n1 global address
    V6PingHelper ping(ifs2.GetAddress(0,1)); // n1's global address on link to R
    ping.SetAttribute("Verbose", BooleanValue(true));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(56));
    ping.SetAttribute("Count", UintegerValue(5));
    ApplicationContainer pingApps = ping.Install(n0);
    pingApps.Start(Seconds(5.0));
    pingApps.Stop(Seconds(25.0));

    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
    Simulator::Destroy();

    // Print final addresses of each node
    std::cout << "\n========== IPv6 Addresses ==========" << std::endl;
    for (uint32_t i = 0; i < hosts.GetN(); ++i) {
        Ptr<Ipv6> ipv6 = hosts.Get(i)->GetObject<Ipv6>();
        std::cout << "Host " << i << ":" << std::endl;
        for (uint32_t j = 0; j < ipv6->GetNInterfaces(); ++j) {
            for (uint32_t k = 0; k < ipv6->GetNAddresses(j); ++k) {
                std::cout << ipv6->GetAddress(j, k).GetAddress() << std::endl;
            }
        }
    }
    Ptr<Ipv6> r6 = router->GetObject<Ipv6>();
    std::cout << "Router:" << std::endl;
    for (uint32_t j = 0; j < r6->GetNInterfaces(); ++j) {
        for (uint32_t k = 0; k < r6->GetNAddresses(j); ++k) {
            std::cout << r6->GetAddress(j, k).GetAddress() << std::endl;
        }
    }
    std::cout << "====================================" << std::endl;

    return 0;
}