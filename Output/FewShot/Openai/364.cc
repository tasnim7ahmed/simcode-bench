#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Logging for Icmpv6L4Protocol to observe Echo requests/replies
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install IPv6 stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Bring up interfaces explicitly (IPv6 address assignment is delayed by default)
    interfaces.SetForwarding(0, true);
    interfaces.SetForwarding(1, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    // ICMPv6 Echo Application from node 0 (client) to node 1 (server)
    uint32_t packetSize = 56;
    uint32_t maxPackets = 5;
    Time interPacketInterval = Seconds(1.0);
    // Use node 1's global IPv6 address
    Ipv6Address destination = interfaces.GetAddress(1, 1);

    V6PingHelper pingHelper(destination);
    pingHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
    pingHelper.SetAttribute("Size", UintegerValue(packetSize));
    pingHelper.SetAttribute("Count", UintegerValue(maxPackets));

    ApplicationContainer pingApps = pingHelper.Install(nodes.Get(0));
    pingApps.Start(Seconds(1.0));
    pingApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}