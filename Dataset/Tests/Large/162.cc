#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

class PersistentTcpConnectionTests : public ns3::TestCase {
public:
    PersistentTcpConnectionTests() : ns3::TestCase("Persistent TCP Connection with NetAnim Tests") {}

    // Test node creation
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(4);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Node container does not contain 4 nodes as expected.");
    }

    // Test PointToPoint link setup for the ring topology
    void TestPointToPointLink() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devices;
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
            devices.Add(temp);
        }

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 4, "Point-to-point link did not create the expected number of devices.");
    }

    // Test the installation of the Internet stack on nodes
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(4);

        InternetStackHelper stack;
        stack.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on node " << i);
        }
    }

    // Test the IP address assignment to devices
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(4);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

        NetDeviceContainer devices;
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
            devices.Add(temp);
        }

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        Ipv4InterfaceContainer interfaces;
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            std::ostringstream subnet;
            subnet << "10.1." << i + 1 << ".0";
            address.SetBase(subnet.str().c_str(), "255.255.255.0");
            interfaces.Add(address.Assign(devices.Get(i)));
        }

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
        nodes.Create(4);

        uint16_t port = 50000;
        Address serverAddress(InetSocketAddress("10.1.1.1", port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));

        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        // Verify that the TCP server is installed and running
        NS_TEST_ASSERT_MSG_EQ(sinkApp.Get(0)->GetState(), Application::STARTED, "TCP server (PacketSink) did not start as expected.");
    }

    // Test TCP client (OnOffHelper) installation
    void TestTcpClientInstallation() {
        NodeContainer nodes;
        nodes.Create(4);

        uint16_t port = 50000;
        Address serverAddress(InetSocketAddress("10.1.1.1", port));
        OnOffHelper client("ns3::TcpSocketFactory", serverAddress);
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", StringValue("2Mbps"));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(nodes.Get(2));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));

        // Verify that the TCP client is installed and running
        NS_TEST_ASSERT_MSG_EQ(clientApp.Get(0)->GetState(), Application::STARTED, "TCP client did not start as expected.");
    }

    // Test NetAnim configuration for node positions
    void TestNetAnimConfiguration() {
        NodeContainer nodes;
        nodes.Create(4);

        AnimationInterface anim("tcp_ring_topology.xml");

        anim.SetConstantPosition(nodes.Get(0), 10.0, 30.0); // Node 0
        anim.SetConstantPosition(nodes.Get(1), 30.0, 50.0); // Node 1
        anim.SetConstantPosition(nodes.Get(2), 50.0, 30.0); // Node 2
        anim.SetConstantPosition(nodes.Get(3), 30.0, 10.0); // Node 3

        // Check if the positions are set correctly for all nodes
        Vector pos0 = anim.GetNodePosition(nodes.Get(0));
        Vector pos1 = anim.GetNodePosition(nodes.Get(1));
        Vector pos2 = anim.GetNodePosition(nodes.Get(2));
        Vector pos3 = anim.GetNodePosition(nodes.Get(3));

        NS_TEST_ASSERT_MSG_EQ(pos0.x, 10.0, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos0.y, 30.0, "Node 0 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.x, 30.0, "Node 1 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos1.y, 50.0, "Node 1 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos2.x, 50.0, "Node 2 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos2.y, 30.0, "Node 2 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos3.x, 30.0, "Node 3 position not set correctly");
        NS_TEST_ASSERT_MSG_EQ(pos3.y, 10.0, "Node 3 position not set correctly");
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
static PersistentTcpConnectionTests g_persistentTcpConnectionTests;
