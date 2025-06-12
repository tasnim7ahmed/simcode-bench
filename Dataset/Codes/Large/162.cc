#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PersistentTcpConnectionWithNetAnim");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("PersistentTcpConnectionWithNetAnim", LOG_LEVEL_INFO);

    // Create 4 nodes for the ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Create a point-to-point link helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    // Create and install point-to-point links in a ring topology
    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.Add(address.Assign(devices.Get(i)));
    }

    // Create TCP server on node 0
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create TCP client on node 2
    OnOffHelper client("ns3::TcpSocketFactory", serverAddress);
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", StringValue("2Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(2));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(9.0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up NetAnim
    AnimationInterface anim("tcp_ring_topology.xml");

    // Set positions for the nodes for visualization in NetAnim
    anim.SetConstantPosition(nodes.Get(0), 10.0, 30.0); // Node 0
    anim.SetConstantPosition(nodes.Get(1), 30.0, 50.0); // Node 1
    anim.SetConstantPosition(nodes.Get(2), 50.0, 30.0); // Node 2
    anim.SetConstantPosition(nodes.Get(3), 30.0, 10.0); // Node 3

    // Enable packet tracing in NetAnim
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

