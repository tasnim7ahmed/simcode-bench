#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologySimulation");

int main(int argc, char *argv[])
{
    LogComponentEnable("TreeTopologySimulation", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> root = nodes.Get(0);
    Ptr<Node> child1 = nodes.Get(1);
    Ptr<Node> child2 = nodes.Get(2);
    Ptr<Node> leaf = nodes.Get(3);

    // Define point-to-point attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links
    // root <-> child1
    NodeContainer n0n1(root, child1);
    // root <-> child2
    NodeContainer n0n2(root, child2);
    // child2 <-> leaf
    NodeContainer n2n3(child2, leaf);

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP Server on root
    uint16_t port = 50000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(root);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Client on leaf
    UdpClientHelper client(i0i1.GetAddress(0), port); // root IP on link 0-1
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(leaf);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(8.0));

    // Logging: Trace packet transmissions at root and leaf
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> packet, const Address &address) {
        NS_LOG_INFO("Root node received a packet of size " << packet->GetSize() << " bytes");
    }));

    Config::ConnectWithoutContext("/NodeList/3/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback([](Ptr<const Packet> packet) {
        NS_LOG_INFO("Leaf node sent a packet of size " << packet->GetSize() << " bytes");
    }));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}