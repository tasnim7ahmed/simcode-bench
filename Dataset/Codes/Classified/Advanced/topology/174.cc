#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologyExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("HybridTopologyExample", LOG_LEVEL_INFO);

    // Create nodes for different topologies
    NodeContainer ringNodes, meshNodes, busNodes, lineNodes, starNodes, treeNodes;
    
    ringNodes.Create(4);  // Ring topology (4 nodes)
    meshNodes.Create(4);  // Mesh topology (4 nodes)
    busNodes.Create(3);   // Bus topology (3 nodes)
    lineNodes.Create(2);  // Line topology (2 nodes)
    starNodes.Create(4);  // Star topology (1 central node + 3 leaf nodes)
    treeNodes.Create(7);  // Tree topology (7 nodes)

    // Create point-to-point helper for links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install Internet stack with OSPF routing
    OspfHelper ospfRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospfRouting, 10);
    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);

    // Install on all nodes
    internet.Install(ringNodes);
    internet.Install(meshNodes);
    internet.Install(busNodes);
    internet.Install(lineNodes);
    internet.Install(starNodes);
    internet.Install(treeNodes);

    // Set up IP addressing
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;

    // --------- Ring Topology ---------
    NetDeviceContainer ringDevices;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        ringDevices = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(ringDevices));
    }

    // --------- Mesh Topology ---------
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        for (uint32_t j = i + 1; j < meshNodes.GetN(); ++j)
        {
            NetDeviceContainer meshDevices = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
            address.SetBase("10.2.1.0", "255.255.255.0");
            interfaces.Add(address.Assign(meshDevices));
        }
    }

    // --------- Bus Topology ---------
    NetDeviceContainer busDevices;
    for (uint32_t i = 0; i < busNodes.GetN() - 1; ++i)
    {
        busDevices = p2p.Install(busNodes.Get(i), busNodes.Get(i + 1));
        address.SetBase("10.3.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(busDevices));
    }

    // --------- Line Topology ---------
    NetDeviceContainer lineDevices = p2p.Install(lineNodes.Get(0), lineNodes.Get(1));
    address.SetBase("10.4.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(lineDevices));

    // --------- Star Topology ---------
    NetDeviceContainer starDevices;
    for (uint32_t i = 1; i < starNodes.GetN(); ++i)
    {
        starDevices = p2p.Install(starNodes.Get(0), starNodes.Get(i));  // Central node is starNodes.Get(0)
        address.SetBase("10.5.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(starDevices));
    }

    // --------- Tree Topology ---------
    // Connecting nodes in a basic binary tree fashion
    NetDeviceContainer treeDevices;
    treeDevices = p2p.Install(treeNodes.Get(0), treeNodes.Get(1));  // Root to left child
    address.SetBase("10.6.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(treeDevices));

    treeDevices = p2p.Install(treeNodes.Get(0), treeNodes.Get(2));  // Root to right child
    address.SetBase("10.7.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(treeDevices));

    treeDevices = p2p.Install(treeNodes.Get(1), treeNodes.Get(3));  // Left child to left grandchild
    address.SetBase("10.8.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(treeDevices));

    treeDevices = p2p.Install(treeNodes.Get(1), treeNodes.Get(4));  // Left child to right grandchild
    address.SetBase("10.9.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(treeDevices));

    treeDevices = p2p.Install(treeNodes.Get(2), treeNodes.Get(5));  // Right child to left grandchild
    address.SetBase("10.10.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(treeDevices));

    treeDevices = p2p.Install(treeNodes.Get(2), treeNodes.Get(6));  // Right child to right grandchild
    address.SetBase("10.11.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(treeDevices));

    // Populate routing tables using OSPF
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up traffic flow
    uint16_t port = 50000;
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    onOffHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = onOffHelper.Install(ringNodes.Get(0)); // Traffic from ringNode 0 to meshNode
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(meshNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up NetAnim
    AnimationInterface anim("hybrid_topology_with_netanim.xml");

    // Set constant positions for better visualization in NetAnim
    anim.SetConstantPosition(ringNodes.Get(0), 10, 10);
    anim.SetConstantPosition(ringNodes.Get(1), 30, 10);
    anim.SetConstantPosition(ringNodes.Get(2), 50, 10);
    anim.SetConstantPosition(ringNodes.Get(3), 70, 10);

    anim.SetConstantPosition(meshNodes.Get(0), 10, 30);
    anim.SetConstantPosition(meshNodes.Get(1), 30, 30);
    anim.SetConstantPosition(meshNodes.Get(2), 50, 30);
    anim.SetConstantPosition(meshNodes.Get(3), 70, 30);

    anim.SetConstantPosition(busNodes.Get(0), 10, 50);
    anim.SetConstantPosition(busNodes.Get(1), 30, 50);
    anim.SetConstantPosition(busNodes.Get(2), 50, 50);

    anim.SetConstantPosition(lineNodes.Get(0), 10, 70);
    anim.SetConstantPosition(lineNodes.Get(1), 30, 70);

    anim.SetConstantPosition(starNodes.Get(0), 50, 70);  // Central node of star
    anim.SetConstantPosition(starNodes.Get(1), 70, 70);
    anim.SetConstantPosition(starNodes.Get(2), 70, 90);
    anim.SetConstantPosition(starNodes.Get(3), 50, 90);

    anim.SetConstantPosition(treeNodes.Get(0), 50, 50);  // Root of tree
    anim.SetConstantPosition(treeNodes.Get(1), 70, 50);
    anim.SetConstantPosition(treeNodes.Get(2), 50, 30);
    anim.SetConstantPosition(treeNodes.Get(3), 70, 30);
    anim.SetConstantPosition(treeNodes.Get(4), 50, 10);
    anim.SetConstantPosition(treeNodes.Get(5), 70, 10);
    anim.SetConstantPosition(treeNodes.Get(6), 90, 10);

    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

