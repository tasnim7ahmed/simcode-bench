#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant components
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6ExtensionFragment", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create six nodes: Src, r1, r2, n0, n1, n2
    ns3::NodeContainer nodes;
    nodes.Create(6);

    ns3::Ptr<ns3::Node> srcNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> r1 = nodes.Get(1);
    ns3::Ptr<ns3::Node> r2 = nodes.Get(2);
    ns3::Ptr<ns3::Node> n0Node = nodes.Get(3);
    ns3::Ptr<ns3::Node> n1Node = nodes.Get(4);
    ns3::Ptr<ns3::Node> n2Node = nodes.Get(5);

    // Install IPv6 Internet Stack on all nodes
    ns3::InternetStackHelper internetv6;
    internetv6.Install(nodes);

    // --- Segment 1: CSMA (Src, r1, n2) with MTU 5000 ---
    ns3::CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma1.SetChannelAttribute("Delay", ns3::StringValue("10ms"));
    csma1.SetDeviceAttribute("Mtu", ns3::UintegerValue(5000));

    ns3::NodeContainer csmaNodes1;
    csmaNodes1.Add(srcNode);
    csmaNodes1.Add(r1);
    csmaNodes1.Add(n2Node);
    ns3::NetDeviceContainer d1 = csma1.Install(csmaNodes1);

    ns3::Ipv6AddressHelper ipv61;
    ipv61.SetBase(ns3::Ipv6Address("2001:db8:1::"), ns3::Ipv6Prefix(64));
    ns3::Ipv6InterfaceContainer iic1 = ipv61.Assign(d1);

    // --- Segment 2: Point-to-Point (r1, r2) with MTU 2000 ---
    ns3::PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p2.SetChannelAttribute("Delay", ns3::StringValue("5ms"));
    p2p2.SetDeviceAttribute("Mtu", ns3::UintegerValue(2000));

    ns3::NodeContainer p2pNodes2;
    p2pNodes2.Add(r1);
    p2pNodes2.Add(r2);
    ns3::NetDeviceContainer d2 = p2p2.Install(p2pNodes2);

    ns3::Ipv6AddressHelper ipv62;
    ipv62.SetBase(ns3::Ipv6Address("2001:db8:2::"), ns3::Ipv6Prefix(64));
    ns3::Ipv6InterfaceContainer iic2 = ipv62.Assign(d2);

    // --- Segment 3: CSMA (r2, n0, n1) with MTU 1500 ---
    ns3::CsmaHelper csma3;
    csma3.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma3.SetChannelAttribute("Delay", ns3::StringValue("10ms"));
    csma3.SetDeviceAttribute("Mtu", ns3::UintegerValue(1500));

    ns3::NodeContainer csmaNodes3;
    csmaNodes3.Add(r2);
    csmaNodes3.Add(n0Node);
    csmaNodes3.Add(n1Node);
    ns3::NetDeviceContainer d3 = csma3.Install(csmaNodes3);

    ns3::Ipv6AddressHelper ipv63;
    ipv63.SetBase(ns3::Ipv6Address("2001:db8:3::"), ns3::Ipv6Prefix(64));
    ns3::Ipv6InterfaceContainer iic3 = ipv63.Assign(d3);

    // Configure IPv6 forwarding on routers
    ns3::Ptr<ns3::Ipv6> ipv6_r1 = r1->GetObject<ns3::Ipv6>();
    ipv6_r1->SetForwarding(0, true); // Interface 0 on r1 (d1)
    ipv6_r1->SetForwarding(1, true); // Interface 1 on r1 (d2)

    ns3::Ptr<ns3::Ipv6> ipv6_r2 = r2->GetObject<ns3::Ipv6>();
    ipv6_r2->SetForwarding(0, true); // Interface 0 on r2 (d2)
    ipv6_r2->SetForwarding(1, true); // Interface 1 on r2 (d3)

    // Populate global routing tables
    ns3::Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // --- Applications ---

    // UDP Echo Server on n0, n1, n2
    ns3::UdpEchoServerHelper echoServer(9); // Port 9

    ns3::ApplicationContainer serverApps_n0 = echoServer.Install(n0Node);
    serverApps_n0.Start(ns3::Seconds(1.0));
    serverApps_n0.Stop(ns3::Seconds(10.0));

    ns3::ApplicationContainer serverApps_n1 = echoServer.Install(n1Node);
    serverApps_n1.Start(ns3::Seconds(1.0));
    serverApps_n1.Stop(ns3::Seconds(10.0));

    ns3::ApplicationContainer serverApps_n2 = echoServer.Install(n2Node);
    serverApps_n2.Start(ns3::Seconds(1.0));
    serverApps_n2.Stop(ns3::Seconds(10.0));

    // UDP Echo Client on Src, sending to n0
    // n0 is at index 1 in the iic3 container (iic3.GetAddress(0) is r2, iic3.GetAddress(1) is n0)
    ns3::UdpEchoClientHelper echoClient(iic3.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(1));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(4000)); // Large packet for fragmentation

    ns3::ApplicationContainer clientApps = echoClient.Install(srcNode);
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    // --- Tracing ---
    ns3::AsciiTraceHelper ascii;
    ns3::Ptr<ns3::OutputStreamWrapper> stream = ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr");
    csma1.EnableAsciiAll(stream);
    p2p2.EnableAsciiAll(stream);
    csma3.EnableAsciiAll(stream);

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(11.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}