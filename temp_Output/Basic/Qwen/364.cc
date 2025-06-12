#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for Icmpv6L4Protocol
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install internet stack with IPv6
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Install Ping application (ICMPv6 Echo Request) from node 0 to node 1
    V4PingHelper ping(interfaces.GetAddress(1, 1)); // Destination IPv6 address
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer clientApp = ping.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}