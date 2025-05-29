#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("V6Ping", LOG_LEVEL_INFO);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Create point-to-point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    // 3. Install devices
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // 4. Install the Internet stack (IPv6)
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Interfaces are brought up by default in ns-3, but link-local is always up
    // We need to assign global addresses as well.
    interfaces.SetForwarding(0, true);  // Enable forwarding if needed
    interfaces.SetDefaultRouteInAllNodes(0); // Set default route in all nodes

    // 6. Install V6Ping application on node 0 (client), ping node 1 (server)
    uint32_t packetSize = 56; // bytes (usual default)
    uint32_t maxPackets = 1000; // large enough so it can run for 10s
    Time interPacketInterval = Seconds(1.0);

    V6PingHelper ping6(interfaces.GetAddress(1, 1)); // [1,1] gets the global address

    ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer app = ping6.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // 7. Enable pcap tracing if desired
    // pointToPoint.EnablePcapAll("two-node-ipv6");

    // 8. Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}