#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    Ptr<Node> n0 = nodes.Get(0); // Central node
    Ptr<Node> n1 = nodes.Get(1); // Branch node 1
    Ptr<Node> n2 = nodes.Get(2); // Branch node 2
    Ptr<Node> n3 = nodes.Get(3); // Branch node 3

    // 2. Configure Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0, n1); // Link N0-N1
    NetDeviceContainer d0d2 = p2p.Install(n0, n2); // Link N0-N2
    NetDeviceContainer d0d3 = p2p.Install(n0, n3); // Link N0-N3

    // 3. Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign unique IP addresses to each network segment
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1); // N0 (0) gets 10.0.0.1, N1 (1) gets 10.0.0.2

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2); // N0 (0) gets 10.0.1.1, N2 (1) gets 10.0.1.2

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i3 = ipv4.Assign(d0d3); // N0 (0) gets 10.0.2.1, N3 (1) gets 10.0.2.2

    // Enable Global Routing (for routing across multiple interfaces of N0)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 5. Configure UDP traffic (Nodes 1, 2, 3 send to Node 0)
    // Server application on Node 0
    uint16_t port = 9; // Echo port

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(n0);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client applications on Nodes 1, 2, 3
    // Get Node 0's IP addresses on each segment for clients to target
    Ipv4Address n0_ip_on_d0d1 = i0i1.GetAddress(0); // N0's IP on 10.0.0.0/24 segment
    Ipv4Address n0_ip_on_d0d2 = i0i2.GetAddress(0); // N0's IP on 10.0.1.0/24 segment
    Ipv4Address n0_ip_on_d0d3 = i0i3.GetAddress(0); // N0's IP on 10.0.2.0/24 segment

    // OnOff client for Node 1 to Node 0
    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(n0_ip_on_d0d1, port));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024)); // 1KB packets
    onoff1.SetAttribute("DataRate", StringValue("5Mbps"));  // 5 Mbps traffic
    ApplicationContainer clientApps1 = onoff1.Install(n1);
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(9.0));

    // OnOff client for Node 2 to Node 0
    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(n0_ip_on_d0d2, port));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));
    onoff2.SetAttribute("DataRate", StringValue("5Mbps"));
    ApplicationContainer clientApps2 = onoff2.Install(n2);
    clientApps2.Start(Seconds(1.5));
    clientApps2.Stop(Seconds(9.5));

    // OnOff client for Node 3 to Node 0
    OnOffHelper onoff3("ns3::UdpSocketFactory", InetSocketAddress(n0_ip_on_d0d3, port));
    onoff3.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff3.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff3.SetAttribute("PacketSize", UintegerValue(1024));
    onoff3.SetAttribute("DataRate", StringValue("5Mbps"));
    ApplicationContainer clientApps3 = onoff3.Install(n3);
    clientApps3.Start(Seconds(2.0));
    clientApps3.Stop(Seconds(10.0));

    // 6. Enable PCAP tracing
    p2p.EnablePcapAll("branch-topology");

    // 7. Run simulation for 10 seconds
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}