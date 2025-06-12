#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create 4 nodes for the ring
    NodeContainer nodes;
    nodes.Create(4);

    // Create all point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the links
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = address.Assign(d01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = address.Assign(d12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = address.Assign(d23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i30 = address.Assign(d30);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Node 0: TCP client (OnOffApplication)
    // Node 2: TCP server (PacketSink)
    uint16_t port = 50000;

    // PacketSink (TCP server) on Node 2
    Address sinkAddress(InetSocketAddress(i12.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(20.0));

    // OnOffApplication (TCP client) on Node 0, sending to 10.1.2.2:50000 (Node 2's port on link to Node 1)
    OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
    client.SetAttribute("DataRate", StringValue("2Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    // Start after sink
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // NetAnim animation
    AnimationInterface anim("ring-topology.xml");

    // Set node positions for visualization (arranged in a ring)
    anim.SetConstantPosition(nodes.Get(0), 100.0, 200.0);
    anim.SetConstantPosition(nodes.Get(1), 200.0, 100.0);
    anim.SetConstantPosition(nodes.Get(2), 300.0, 200.0);
    anim.SetConstantPosition(nodes.Get(3), 200.0, 300.0);

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}