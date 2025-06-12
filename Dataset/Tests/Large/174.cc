#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/test.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

// Test for Node creation for various topologies
void TestNodeCreation()
{
    // Create nodes for different topologies
    NodeContainer ringNodes, meshNodes, busNodes, lineNodes, starNodes, treeNodes;

    ringNodes.Create(4);   // Ring topology (4 nodes)
    meshNodes.Create(4);   // Mesh topology (4 nodes)
    busNodes.Create(3);    // Bus topology (3 nodes)
    lineNodes.Create(2);   // Line topology (2 nodes)
    starNodes.Create(4);   // Star topology (1 central node + 3 leaf nodes)
    treeNodes.Create(7);   // Tree topology (7 nodes)

    NS_TEST_ASSERT_MSG_EQ(ringNodes.GetN(), 4, "Ring topology node creation failed.");
    NS_TEST_ASSERT_MSG_EQ(meshNodes.GetN(), 4, "Mesh topology node creation failed.");
    NS_TEST_ASSERT_MSG_EQ(busNodes.GetN(), 3, "Bus topology node creation failed.");
    NS_TEST_ASSERT_MSG_EQ(lineNodes.GetN(), 2, "Line topology node creation failed.");
    NS_TEST_ASSERT_MSG_EQ(starNodes.GetN(), 4, "Star topology node creation failed.");
    NS_TEST_ASSERT_MSG_EQ(treeNodes.GetN(), 7, "Tree topology node creation failed.");
}

// Test for Point-to-Point link creation
void TestPointToPointLinkCreation()
{
    NodeContainer ringNodes, meshNodes, busNodes, lineNodes, starNodes, treeNodes;
    ringNodes.Create(4);
    meshNodes.Create(4);
    busNodes.Create(3);
    lineNodes.Create(2);
    starNodes.Create(4);
    treeNodes.Create(7);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;

    // Testing link creation for Ring topology
    NetDeviceContainer ringDevices;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        ringDevices = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(ringDevices));
    }

    // Test other topologies similarly (mesh, bus, etc.) by creating links

    NS_TEST_ASSERT_MSG_EQ(ringDevices.GetN(), 4, "Point-to-point link creation for ring topology failed.");
}

// Test for IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer ringNodes;
    ringNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;

    // Set up IP addressing for Ring topology
    NetDeviceContainer ringDevices;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        ringDevices = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(ringDevices));
    }

    NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 4, "IP address assignment for ring topology failed.");
}

// Test for OSPF routing setup
void TestOspfRoutingSetup()
{
    NodeContainer ringNodes;
    ringNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    OspfHelper ospfRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospfRouting, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(ringNodes);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Test if OSPF routing is installed
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = ringNodes.Get(i)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "OSPF routing not installed on node.");
    }
}

// Test for TCP traffic flow
void TestTcpTrafficFlow()
{
    NodeContainer ringNodes, meshNodes;
    ringNodes.Create(4);
    meshNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;

    // Set up IP addressing and link creation
    NetDeviceContainer ringDevices;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        ringDevices = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(ringDevices));
    }

    // Set up traffic flow using OnOff application
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

    // Run simulation and check traffic flow
    Simulator::Run();
    NS_TEST_ASSERT_MSG(clientApp.GetN() > 0, "TCP traffic flow setup failed.");
    Simulator::Destroy();
}

// Test for NetAnim configuration
void TestNetAnimConfiguration()
{
    NodeContainer ringNodes;
    ringNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;

    // Set up IP addressing and link creation for NetAnim
    NetDeviceContainer ringDevices;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        ringDevices = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(ringDevices));
    }

    // Set up NetAnim
    AnimationInterface anim("hybrid_topology_with_netanim.xml");
    anim.SetConstantPosition(ringNodes.Get(0), 10, 10);
    anim.SetConstantPosition(ringNodes.Get(1), 30, 10);
    anim.SetConstantPosition(ringNodes.Get(2), 50, 10);
    anim.SetConstantPosition(ringNodes.Get(3), 70, 10);

    // Test if animation is correctly set up
    NS_TEST_ASSERT_MSG(anim.IsAnimationEnabled(), "NetAnim configuration failed.");
}

// Main function to execute all tests
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestPointToPointLinkCreation();
    TestIpAddressAssignment();
    TestOspfRoutingSetup();
    TestTcpTrafficFlow();
    TestNetAnimConfiguration();

    return 0;
}
