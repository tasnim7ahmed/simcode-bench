#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

class OspfWithNetAnimTests : public ns3::TestCase {
public:
    OspfWithNetAnimTests() : ns3::TestCase("OSPF with NetAnim Tests") {}

    // Test node creation
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(4);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Node container does not contain 4 nodes as expected.");
    }

    // Test point-to-point link setup between nodes
    void TestPointToPointLinks() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer d1d2, d2d3, d3d4, d4d1;
        d1d2 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        d2d3 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
        d3d4 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
        d4d1 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

        NS_TEST_ASSERT_MSG_EQ(d1d2.GetN(), 2, "Link between node 0 and node 1 not installed properly.");
        NS_TEST_ASSERT_MSG_EQ(d2d3.GetN(), 2, "Link between node 1 and node 2 not installed properly.");
        NS_TEST_ASSERT_MSG_EQ(d3d4.GetN(), 2, "Link between node 2 and node 3 not installed properly.");
        NS_TEST_ASSERT_MSG_EQ(d4d1.GetN(), 2, "Link between node 3 and node 0 not installed properly.");
    }

    // Test OSPF routing installation
    void TestOspfRoutingInstallation() {
        NodeContainer nodes;
        nodes.Create(4);

        OspfHelper ospfRouting;
        Ipv4ListRoutingHelper listRouting;
        listRouting.Add(ospfRouting, 10); // OSPF priority 10
        InternetStackHelper stack;
        stack.SetRoutingHelper(listRouting);
        stack.Install(nodes);

        // Check if OSPF is correctly installed by verifying the routing table
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            Ptr<Ipv4RoutingProtocol> routingProtocol = ipv4->GetRoutingProtocol();

            NS_TEST_ASSERT_MSG_NE(routingProtocol, nullptr, "OSPF routing protocol not installed on node " << i);
        }
    }

    // Test IP address assignment
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer d1d2, d2d3, d3d4, d4d1;
        d1d2 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        d2d3 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
        d3d4 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
        d4d1 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        Ipv4InterfaceContainer interfaces;

        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(d1d2));

        address.SetBase("10.2.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(d2d3));

        address.SetBase("10.3.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(d3d4));

        address.SetBase("10.4.1.0", "255.255.255.0");
        interfaces.Add(address.Assign(d4d1));

        // Verify IP addresses are assigned
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            Ipv4InterfaceContainer interfaceContainer = ipv4->GetObject<Ipv4L3Protocol>()->GetInterfaces();
            NS_TEST_ASSERT_MSG_NE(interfaceContainer.GetAddress(0), Ipv4Address("0.0.0.0"), "Node " << i << " does not have a valid IP address assigned.");
        }
    }

    // Test TCP traffic generation from node 0 to node 2
    void TestTcpTrafficGeneration() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer d1d2, d2d3, d3d4, d4d1;
        d1d2 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        d2d3 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
        d3d4 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
        d4d1 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        address.Assign(d1d2);
        address.SetBase("10.2.1.0", "255.255.255.0");
        address.Assign(d2d3);
        address.SetBase("10.3.1.0", "255.255.255.0");
        address.Assign(d3d4);
        address.SetBase("10.4.1.0", "255.255.255.0");
        address.Assign(d4d1);

        uint16_t port = 50000;
        OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress("10.3.1.2", port));
        onOffHelper.SetAttribute("DataRate", StringValue("5Mbps"));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();
    }

    // Test NetAnim configuration for node positions
    void TestNetAnimConfiguration() {
        NodeContainer nodes;
        nodes.Create(4);

        AnimationInterface anim("ospf_with_netanim.xml");

        anim.SetConstantPosition(nodes.Get(0), 10, 10);  // Node 0
        anim.SetConstantPosition(nodes.Get(1), 30, 10);  // Node 1
        anim.SetConstantPosition(nodes.Get(2), 30, 30);  // Node 2
        anim.SetConstantPosition(nodes.Get(3), 10, 30);  // Node 3

        // Check if the positions are set correctly for the nodes
        Vector pos0 = anim.GetNodePosition(nodes.Get(0));
        Vector pos1 = anim.GetNodePosition(nodes.Get(1));
        Vector pos2 = anim.GetNodePosition(nodes.Get(2));
        Vector pos3 = anim.GetNodePosition(nodes.Get(3));

        NS_TEST_ASSERT_MSG_EQ(pos0.x, 10, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos0.y, 10, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.x, 30, "Node 1 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.y, 10, "Node 1 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos2.x, 30, "Node 2 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos2.y, 30, "Node 2 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos3.x, 10, "Node 3 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos3.y, 30, "Node 3 position not set correctly");
    }

    virtual void DoRun() {
        TestNodeCreation();
        TestPointToPointLinks();
        TestOspfRoutingInstallation();
        TestIpAddressAssignment();
        TestTcpTrafficGeneration();
        TestNetAnimConfiguration();
    }
};

// Register the test suite
static OspfWithNetAnimTests g_ospfWithNetAnimTests;
