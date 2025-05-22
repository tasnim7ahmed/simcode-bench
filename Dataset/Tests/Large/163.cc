#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

class NonPersistentTcpWithNetAnimTests : public ns3::TestCase {
public:
    NonPersistentTcpWithNetAnimTests() : ns3::TestCase("Non-Persistent TCP with NetAnim Tests") {}

    // Test node creation
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node container does not contain 2 nodes as expected.");
    }

    // Test PointToPoint link setup between two nodes
    void TestPointToPointLink() {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-point link did not create the expected number of devices.");
    }

    // Test the installation of the Internet stack on the nodes
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on node " << i);
        }
    }

    // Test IP address assignment to the devices
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Check if each node has been assigned an IP address
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            Ipv4InterfaceContainer interfaceContainer = ipv4->GetObject<Ipv4L3Protocol>()->GetInterfaces();
            NS_TEST_ASSERT_MSG_NE(interfaceContainer.GetAddress(0), Ipv4Address("0.0.0.0"), "Node " << i << " does not have a valid IP address assigned.");
        }
    }

    // Test TCP server (PacketSink) installation
    void TestTcpServerInstallation() {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 50000;
        Address serverAddress(InetSocketAddress("10.1.1.1", port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));

        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(5.0));

        // Verify that the TCP server is installed and running
        NS_TEST_ASSERT_MSG_EQ(sinkApp.Get(0)->GetState(), Application::STARTED, "TCP server (PacketSink) did not start as expected.");
    }

    // Test TCP client (OnOffHelper) installation for non-persistent connection
    void TestTcpClientInstallation() {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 50000;
        Address serverAddress(InetSocketAddress("10.1.1.1", port));
        OnOffHelper client("ns3::TcpSocketFactory", serverAddress);
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Non-persistent connection
        client.SetAttribute("DataRate", StringValue("2Mbps"));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(nodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(4.0));

        // Verify that the TCP client is installed and running
        NS_TEST_ASSERT_MSG_EQ(clientApp.Get(0)->GetState(), Application::STARTED, "TCP client did not start as expected.");
    }

    // Test NetAnim configuration for node positions
    void TestNetAnimConfiguration() {
        NodeContainer nodes;
        nodes.Create(2);

        AnimationInterface anim("non_persistent_tcp_netanim.xml");

        anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0); // Server (Node 0)
        anim.SetConstantPosition(nodes.Get(1), 30.0, 20.0); // Client (Node 1)

        // Check if the positions are set correctly for both nodes
        Vector pos0 = anim.GetNodePosition(nodes.Get(0));
        Vector pos1 = anim.GetNodePosition(nodes.Get(1));

        NS_TEST_ASSERT_MSG_EQ(pos0.x, 10.0, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos0.y, 20.0, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.x, 30.0, "Node 1 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.y, 20.0, "Node 1 position not set correctly");
    }

    virtual void DoRun() {
        TestNodeCreation();
        TestPointToPointLink();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestTcpServerInstallation();
        TestTcpClientInstallation();
        TestNetAnimConfiguration();
    }
};

// Register the test suite
static NonPersistentTcpWithNetAnimTests g_nonPersistentTcpWithNetAnimTests;
