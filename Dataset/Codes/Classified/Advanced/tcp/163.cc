#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NonPersistentTcpWithNetAnim");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("NonPersistentTcpWithNetAnim", LOG_LEVEL_INFO);

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link with 5Mbps bandwidth and 1ms delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install the link between the nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create a TCP server on node 0
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(5.0));  // Stop after 5 seconds

    // Create a TCP client on node 1 for a non-persistent connection
    OnOffHelper client("ns3::TcpSocketFactory", serverAddress);
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Non-persistent connection
    client.SetAttribute("DataRate", StringValue("2Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));  // Start client 1 second after the server
    clientApp.Stop(Seconds(4.0));   // Stop client after the connection finishes

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up NetAnim
    AnimationInterface anim("non_persistent_tcp_netanim.xml");

    // Set node positions for visualization
    anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0); // Server (Node 0)
    anim.SetConstantPosition(nodes.Get(1), 30.0, 20.0); // Client (Node 1)

    // Enable packet metadata for NetAnim
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

