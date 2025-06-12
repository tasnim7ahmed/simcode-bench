#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-ospf-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set up logging if needed
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4); // Node 0, 1, 2, 3 arranged in a square

    // Names for easier use
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Node> n2 = nodes.Get(2);
    Ptr<Node> n3 = nodes.Get(3);

    // Create point-to-point links
    // Square topology: 0--1
    //                  |  |
    //                  3--2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // The links: 0<->1, 1<->2, 2<->3, 3<->0
    NetDeviceContainer nd_0_1 = p2p.Install(n0, n1);
    NetDeviceContainer nd_1_2 = p2p.Install(n1, n2);
    NetDeviceContainer nd_2_3 = p2p.Install(n2, n3);
    NetDeviceContainer nd_3_0 = p2p.Install(n3, n0);

    // Enable pcap tracing on all links
    p2p.EnablePcapAll("square-ospf");

    // Install Internet stack with OSPFv2
    InternetStackHelper internet;
    Ipv4OspfHelper ospfRouting;

    internet.SetRoutingHelper(ospfRouting);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_0_1 = ip.Assign(nd_0_1);

    ip.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_1_2 = ip.Assign(nd_1_2);

    ip.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_2_3 = ip.Assign(nd_2_3);

    ip.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_3_0 = ip.Assign(nd_3_0);

    // Install UDP Server on node 2
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(n2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Client on node 0 -> send to node 2
    UdpClientHelper client(if_1_2.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(n0);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(8.0));

    // Enable ASCII tracing if needed (commented out)
    // AsciiTraceHelper ascii;
    // p2p.EnableAsciiAll(ascii.CreateFileStream("square-ospf.tr"));

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Show routing tables at the end
    Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(10.0), std::cout);

    Simulator::Destroy();
    return 0;
}