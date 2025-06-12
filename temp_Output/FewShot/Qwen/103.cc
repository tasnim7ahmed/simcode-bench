#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_INFO);

    // Create three nodes: n0 (client), r (router), n1 (server)
    NodeContainer nodes;
    nodes.Create(3);

    // Create two CSMA channels to connect n0 <-> r and r <-> n1
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Install CSMA devices on node pairs
    NetDeviceContainer dev0r = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    NetDeviceContainer devr1 = csma.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));

    // Install IPv6 internet stack on all nodes
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    // Network 1: n0 - r
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifc0r = ipv6.Assign(dev0r);
    ifc0r.SetForwarding(1, true); // enable forwarding on router side

    // Network 2: r - n1
    ipv6.SetBase(Ipv6Address("2001:db9::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifcr1 = ipv6.Assign(devr1);
    ifcr1.SetForwarding(0, true); // enable forwarding on router side

    // Set default routes on n0 and n1 via router
    Ptr<Ipv6> ipv6n0 = nodes.Get(0)->GetObject<Ipv6>();
    Ipv6InterfaceAddress iaddrn0 = ifc0r.GetAddress(0, 1);
    Ipv6Address routern0 = ifc0r.GetAddress(1, 1).GetAddress();
    Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:db9::"), Ipv6Prefix(64), routern0, 1);

    Ptr<Ipv6> ipv6n1 = nodes.Get(2)->GetObject<Ipv6>();
    Ipv6InterfaceAddress iaddrn1 = ifcr1.GetAddress(1, 1);
    Ipv6Address routern1 = ifcr1.GetAddress(0, 1).GetAddress();
    Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:db8::"), Ipv6Prefix(64), routern1, 1);

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("basic-ipv6-routing.tr");
    csma.EnableAsciiAll(stream);
    csma.EnablePcapAll("basic-ipv6-routing");

    // Setup Ping6 application from n0 to n1
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping6;
    ping6.SetLocal(nodes.Get(0)->GetObject<Ipv6>());
    ping6.SetRemote(ifcr1.GetAddress(1, 1).GetAddress()); // address of n1
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Print the routing table of n0
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> routingTable = routingHelper.GetStaticRouting(ipv6n0);
    std::cout << "Routing Table for n0:" << std::endl;
    for (uint32_t i = 0; i < routingTable->GetNRoutes(); ++i) {
        Ipv6RoutingTableEntry route = routingTable->GetRoute(i);
        std::cout << route << std::endl;
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}