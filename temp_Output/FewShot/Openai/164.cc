#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for critical OSPF events (optional, can be adjusted)
    LogComponentEnable("OspfRouter", LOG_LEVEL_INFO);

    // Set up the simulation parameters
    uint32_t numNodes = 5;
    double simTime = 20.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create point-to-point links between the nodes to form a partial mesh
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect node 0 <-> 1, 1 <-> 2, 2 <-> 3, 3 <-> 4, and 0 <-> 4 to form a ring
    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d34 = p2p.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer d40 = p2p.Install(nodes.Get(4), nodes.Get(0));

    // Install OSPF on all nodes
    InternetStackHelper stack;
    OspfRoutingHelper ospf;
    stack.SetRoutingHelper(ospf);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = address.Assign(d01);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = address.Assign(d12);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = address.Assign(d23);

    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i34 = address.Assign(d34);

    address.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i40 = address.Assign(d40);

    // Give each node a position for NetAnim visualization
    AnimationInterface anim("ospf-netanim.xml");
    anim.SetConstantPosition(nodes.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 150.0, 50.0);
    anim.SetConstantPosition(nodes.Get(2), 200.0, 150.0);
    anim.SetConstantPosition(nodes.Get(3), 150.0, 250.0);
    anim.SetConstantPosition(nodes.Get(4), 50.0, 250.0);

    // Install applications: send UDP traffic from node 0 to node 3
    uint16_t port = 4000;
    // UDP Server on node 3
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(3));
    serverApp.Start(Seconds(2.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Client on node 0
    // Use destination interface of node 3 from link 2-3, as it is connected to others via OSPF
    Ipv4Address dstAddr = i23.GetAddress(1);

    UdpClientHelper client(dstAddr, port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(4.0));
    clientApp.Stop(Seconds(simTime));

    // Enable packet metadata for visualization
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}