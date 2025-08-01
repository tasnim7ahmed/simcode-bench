
// Network topology
// //
// //             n0   r    n1
// //             |    _    |
// //             ====|_|====
// //                router
// //
// // - Tracing of queues and packet receptions to file "simple-routing-ping6.tr"

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleRoutingPing6Example");

/**
 * \class StackHelper
 * \brief Helper to set or get some IPv6 information about nodes.
 */
class StackHelper
{
  public:
    /**
     * \brief Add an address to a IPv6 node.
     * \param n node
     * \param interface interface index
     * \param address IPv6 address to add
     */
    inline void AddAddress(Ptr<Node>& n, uint32_t interface, Ipv6Address address)
    {
        Ptr<Ipv6> ipv6 = n->GetObject<Ipv6>();
        ipv6->AddAddress(interface, address);
    }

    /**
     * \brief Print the routing table.
     * \param n the node
     */
    inline void PrintRoutingTable(Ptr<Node>& n)
    {
        Ptr<Ipv6StaticRouting> routing = nullptr;
        Ipv6StaticRoutingHelper routingHelper;
        Ptr<Ipv6> ipv6 = n->GetObject<Ipv6>();
        uint32_t nbRoutes = 0;
        Ipv6RoutingTableEntry route;

        routing = routingHelper.GetStaticRouting(ipv6);

        std::cout << "Routing table of " << n << " : " << std::endl;
        std::cout << "Destination\t\t\t\t"
                  << "Gateway\t\t\t\t\t"
                  << "Interface\t"
                  << "Prefix to use" << std::endl;

        nbRoutes = routing->GetNRoutes();
        for (uint32_t i = 0; i < nbRoutes; i++)
        {
            route = routing->GetRoute(i);
            std::cout << route.GetDest() << "\t" << route.GetGateway() << "\t"
                      << route.GetInterface() << "\t" << route.GetPrefixToUse() << "\t"
                      << std::endl;
        }
    }
};

int
main(int argc, char** argv)
{
#if 0
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_ALL);
  LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv6StaticRouting", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv6Interface", LOG_LEVEL_ALL);
  LogComponentEnable ("Ping", LOG_LEVEL_ALL);
#endif

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    StackHelper stackHelper;

    NS_LOG_INFO("Create nodes.");
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);
    NodeContainer all(n0, r, n1);

    NS_LOG_INFO("Create IPv6 Internet Stack");
    InternetStackHelper internetv6;
    internetv6.Install(all);

    NS_LOG_INFO("Create channels.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1 = csma.Install(net1);
    NetDeviceContainer d2 = csma.Install(net2);

    NS_LOG_INFO("Create networks and assign IPv6 Addresses.");
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(d1);
    i1.SetForwarding(1, true);
    i1.SetDefaultRouteInAllNodes(1);
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i2 = ipv6.Assign(d2);
    i2.SetForwarding(0, true);
    i2.SetDefaultRouteInAllNodes(0);

    stackHelper.PrintRoutingTable(n0);

    /* Create a Ping application to send ICMPv6 echo request from n0 to n1 via r */
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    PingHelper ping(i2.GetAddress(1, 1));

    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ApplicationContainer apps = ping.Install(net1.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(20.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("simple-routing-ping6.tr"));
    csma.EnablePcapAll("simple-routing-ping6", true);

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
