#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-global-routing-helper.h"
#include "ns3/netanim-module.h"

// Define constants for the simulation
const uint32_t N_PACKETS = 1;      // Number of packets to send
const uint32_t PACKET_SIZE = 2000; // Size of packets (larger than CSMA MTU for fragmentation)
const uint16_t UDP_ECHO_PORT = 9;  // UDP Echo server port

int main()
{
    // 1. Enable logging for relevant components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL); // To see fragmentation/reassembly
    LogComponentEnable("Ipv6EndPoint", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6Fragmentation", LOG_LEVEL_ALL); // Specifically for fragmentation logs

    // Set simulation time resolution
    Time::SetResolution(Time::NS);

    // 2. Create nodes: n0 (client), r (router), n1 (server)
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    // 3. Configure CSMA channels
    CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csmaHelper.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install CSMA devices on n0 and r
    NetDeviceContainer devices0r;
    devices0r = csmaHelper.Install(NodeContainer(n0, r));

    // Install CSMA devices on r and n1
    NetDeviceContainer devicesr1;
    devicesr1 = csmaHelper.Install(NodeContainer(r, n1));

    // 4. Install IPv6 Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    // Subnet 1: 2001:db8:1::/64 for n0 and r
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if0r = ipv6.Assign(devices0r);
    // Enable IPv6 forwarding on the router's interface connected to n0
    if0r.SetForwarding(1, true); // if0r.Get(1) is the router's device

    // Subnet 2: 2001:db8:2::/64 for r and n1
    ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifr1 = ipv6.Assign(devicesr1);
    // Enable IPv6 forwarding on the router's interface connected to n1
    ifr1.SetForwarding(0, true); // ifr1.Get(0) is the router's device

    // Populate IPv6 global routing tables
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Create and install UDP Echo Server application on n1
    UdpEchoServerHelper echoServer(UDP_ECHO_PORT);
    ApplicationContainer serverApps = echoServer.Install(n1);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // 7. Create and install UDP Echo Client application on n0
    // Client targets n1's IPv6 address on the 2001:db8:2:: network
    // ifr1.GetAddress(1) corresponds to n1's IPv6 address
    UdpEchoClientHelper echoClient(ifr1.GetAddress(1), UDP_ECHO_PORT);
    echoClient.SetAttribute("MaxPackets", UintegerValue(N_PACKETS));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send one packet per second
    echoClient.SetAttribute("PacketSize", UintegerValue(PACKET_SIZE)); // Large packet size for fragmentation

    ApplicationContainer clientApps = echoClient.Install(n0);
    clientApps.Start(Seconds(2.0)); // Start after server
    clientApps.Stop(Seconds(10.0));

    // 8. Enable tracing
    // Capture packet traces using pcap
    csmaHelper.EnablePcapAll("fragmentation-ipv6", false); // "false" means not to append, overwrite

    // Save traces of queue and packet reception events to an ASCII trace file
    // EnableAsciiAll captures events at the MAC and IP layers, including sends, receives, and drops,
    // which covers "packet reception events" and provides insights into "queue" behavior.
    AsciiTraceHelper ascii;
    csmaHelper.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6.tr"));

    // 9. Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}