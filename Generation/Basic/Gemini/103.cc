#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-global-routing-helper.h"

NS_LOG_COMPONENT_DEFINE("Ipv6CsmaPing");

int main(int argc, char* argv[])
{
    // Set time resolution to nanoseconds for precise simulation
    Time::SetResolution(Time::NS);

    // Enable logging for specific components to observe their behavior
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6GlobalRouting", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO); // Log IPv6 Layer 3 protocol events (e.g., forwarding)

    // 1. Create nodes: n0 (source), r (router), n1 (destination)
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> n0 = nodes.Get(0); // Source node for ping
    Ptr<Node> r = nodes.Get(1);  // Router node
    Ptr<Node> n1 = nodes.Get(2); // Destination node for ping

    // 2. Install IPv6 Internet Stack on all nodes
    Ipv6InternetStackHelper ipv6StackHelper;
    ipv6StackHelper.Install(nodes);

    // 3. Setup CSMA channels with specified data rate and delay
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps")); // Set data rate to 100 Mbps
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(10))); // Set propagation delay to 10 microseconds

    // Create LAN 1: Connect n0 and r
    NetDeviceContainer devices0r = csma.Install(NodeContainer(n0, r));

    // Create LAN 2: Connect r and n1
    NetDeviceContainer devicesrn1 = csma.Install(NodeContainer(r, n1));

    // 4. Assign IPv6 addresses to network interfaces
    Ipv6AddressHelper ipv6;

    // Assign addresses for LAN 1 (n0-r link) from subnet 2001:db8:1::/64
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaces0r = ipv6.Assign(devices0r);
    // ifaces0r.GetAddress(0,0) will be n0's address (2001:db8:1::1)
    // ifaces0r.GetAddress(1,0) will be r's address on LAN1 (2001:db8:1::2)

    // Assign addresses for LAN 2 (r-n1 link) from subnet 2001:db8:2::/64
    ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifacesrn1 = ipv6.Assign(devicesrn1);
    // ifacesrn1.GetAddress(0,0) will be r's address on LAN2 (2001:db8:2::1)
    // ifacesrn1.GetAddress(1,0) will be n1's address (2001:db8:2::2)

    // 5. Configure forwarding on the router (r)
    // This enables the router node to forward packets between its interfaces.
    Ptr<Ipv6> ipv6r = r->GetObject<Ipv6>();
    ipv6r->SetForwarding(true);

    // 6. Populate IPv6 routing tables for all nodes
    // This helper automatically sets up static routes for directly connected networks.
    // It also sets up default routes for hosts and forwarding routes for routers.
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // 7. Configure Ping6 application on n0 to send ICMPv6 echo requests to n1
    Ping6Helper ping6;
    // Set the remote (destination) IPv6 address to n1's address
    Ipv6Address n1Ipv6Address = ifacesrn1.GetAddress(1, 0); // n1 is the second device in devicesrn1 (index 1)
    ping6.SetRemote(n1Ipv6Address);
    ping6.SetAttribute("MaxPackets", UintegerValue(4));   // Send 4 ping packets
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval between pings
    ping6.SetAttribute("PacketSize", UintegerValue(128)); // Ping packet size of 128 bytes

    ApplicationContainer pingApps = ping6.Install(n0); // Install the ping application on n0
    pingApps.Start(Seconds(1.0)); // Start sending pings at 1 second
    pingApps.Stop(Seconds(10.0)); // Stop the application at 10 seconds

    // 8. Enable ASCII and PCAP tracing to log the simulation to files
    // ASCII trace for all CSMA devices (detailed event log)
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ipv6-csma-ping.tr"));
    // PCAP trace for all CSMA devices (packet capture for Wireshark analysis)
    csma.EnablePcapAll("ipv6-csma-ping", true); // true for promiscuous mode, capturing all traffic on the channel

    // 9. Print the IPv6 routing table of node n0
    // Schedule this at 0.5 seconds, before the ping application starts, to see the initial routes.
    Simulator::Schedule(Seconds(0.5), &Ipv6RoutingHelper::PrintRoutingTable, n0->GetObject<Ipv6>(), 0); // Print all interfaces' routes (index 0)

    // Set the total simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}