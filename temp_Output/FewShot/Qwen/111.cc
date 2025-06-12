#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd.h"
#include "ns3/radvd-interface.h"
#include "ns3/radvd-prefix.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging
    LogComponentEnable("RadvdApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer nodes(n0, r, n1);

    // Create CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Install CSMA devices
    NetDeviceContainer dev_n0_r = csma.Install(NodeContainer(n0, r));
    NetDeviceContainer dev_r_n1 = csma.Install(NodeContainer(r, n1));

    // Install Internet stack with IPv6 support
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    // Assign IPv6 addresses to interfaces
    Ipv6AddressHelper ipv6;

    // Subnet 2001:1::/64 for n0's network
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_n0_r = ipv6.Assign(dev_n0_r);
    i_n0_r.SetForwarding(1, true); // Enable forwarding on router interface

    // Subnet 2001:2::/64 for n1's network
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_r_n1 = ipv6.Assign(dev_r_n1);
    i_r_n1.SetForwarding(0, true); // Enable forwarding on router interface

    // Configure Radvd on the router
    RadvdHelper radvdHelper;

    // Interface 0 (connected to n0)
    uint32_t interfaceN0 = 0;
    radvdHelper.AddAnnouncedInterface(r, interfaceN0);
    radvdHelper.AddDefaultPrefix(r, interfaceN0, Ipv6Prefix(64), RadvdPrefix::DEFAULT);

    // Interface 1 (connected to n1)
    uint32_t interfaceN1 = 1;
    radvdHelper.AddAnnouncedInterface(r, interfaceN1);
    radvdHelper.AddDefaultPrefix(r, interfaceN1, Ipv6Prefix(64), RadvdPrefix::DEFAULT);

    ApplicationContainer radvdApp = radvdHelper.Install(r);
    radvdApp.Start(Seconds(1.0));
    radvdApp.Stop(Seconds(10.0));

    // Ping application from n0 to n1
    Ping6Helper ping6;
    ping6.SetLocal(n0, Ipv6Address::GetAllNodesMulticast());
    ping6.SetRemote(Ipv6Address("2001:2::200:ff:fe00:4")); // MAC-based address of n1
    ping6.SetAttribute("MaxPackets", UintegerValue(5));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps = ping6.Install(n0);
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    // Set up tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("radvd.tr");
    csma.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}