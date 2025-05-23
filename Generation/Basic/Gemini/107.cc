#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant components to observe fragmentation and events
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("Ipv6L3Protocol", ns3::LOG_LEVEL_ALL); // To see IPv6 fragmentation messages
    ns3::LogComponentEnable("CsmaNetDevice", ns3::LOG_LEVEL_ALL); // To see packet Rx/Tx/Queue events

    // Allow user to override default attributes
    ns3::CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create three nodes: n0 (client), r (router), n1 (server)
    ns3::NodeContainer nodes;
    nodes.Create(3);
    ns3::Ptr<ns3::Node> n0 = nodes.Get(0); // Client node
    ns3::Ptr<ns3::Node> r = nodes.Get(1);  // Router node
    ns3::Ptr<ns3::Node> n1 = nodes.Get(2); // Server node

    // Create CSMA links
    ns3::CsmaHelper csmaHelper1;
    csmaHelper1.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csmaHelper1.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    // Link between n0 and r has MTU 5000
    csmaHelper1.SetDeviceAttribute("Mtu", ns3::UintegerValue(5000));
    ns3::NetDeviceContainer devices1 = csmaHelper1.Install(ns3::NodeContainer(n0, r));

    ns3::CsmaHelper csmaHelper2;
    csmaHelper2.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csmaHelper2.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    // Link between r and n1 has MTU 1500
    csmaHelper2.SetDeviceAttribute("Mtu", ns3::UintegerValue(1500));
    ns3::NetDeviceContainer devices2 = csmaHelper2.Install(ns3::NodeContainer(r, n1));

    // Install IPv6 Internet Stack on all nodes
    ns3::Ipv6InternetStackHelper ipv6Stack;
    ipv6Stack.Install(nodes);

    // Assign IPv6 addresses
    ns3::Ipv6AddressHelper ipv6AddrHelper1;
    ipv6AddrHelper1.SetBase("2001:db8:1::", ns3::Ipv6Prefix(64));
    ns3::Ipv6InterfaceContainer if0_r = ipv6AddrHelper1.Assign(devices1);

    ns3::Ipv6AddressHelper ipv6AddrHelper2;
    ipv6AddrHelper2.SetBase("2001:db8:2::", ns3::Ipv6Prefix(64));
    ns3::Ipv6InterfaceContainer r_if1 = ipv6AddrHelper2.Assign(devices2);

    // Enable IPv6 forwarding on the router (node r)
    // For devices1: index 0 is n0, index 1 is r. Set forwarding on r's device for this net.
    if0_r.SetForwarding(1, true); 
    // For devices2: index 0 is r, index 1 is n1. Set forwarding on r's device for this net.
    r_if1.SetForwarding(0, true);

    // Populate routing tables. This is crucial for the router to know how to reach n1 from n0.
    ns3::Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP Echo Server on n1
    ns3::UdpEchoServerHelper echoServer(9); // Port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(n1);
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Setup UDP Echo Client on n0, targeting n1
    // Get n1's IPv6 address from the r_if1 interface container (n1 is at index 1 of devices2)
    ns3::Ipv6Address n1Ipv6Address = r_if1.GetAddress(1); 
    ns3::UdpEchoClientHelper echoClient(n1Ipv6Address, 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    // Set packet size to be larger than the MTU of the second link (1500)
    // This will cause fragmentation on the r-n1 link.
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(2000)); 
    ns3::ApplicationContainer clientApps = echoClient.Install(n0);
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    // Enable tracing of queues and packet receptions
    // Output trace file name: 'fragmentation-ipv6-two-mtu.tr'
    ns3::AsciiTraceHelper ascii;
    csmaHelper1.EnableAsciiAll(ascii, "fragmentation-ipv6-two-mtu.tr");
    csmaHelper2.EnableAsciiAll(ascii, "fragmentation-ipv6-two-mtu.tr");

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(11.0));

    // Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}