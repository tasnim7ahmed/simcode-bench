#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Define a test suite for the P2P TCP simulation
class P2PTcpTestSuite : public TestSuite
{
public:
    P2PTcpTestSuite() : TestSuite("p2p-tcp-tests", UNIT)
    {
        AddTestCase(new TestNodeCreation(), TestCase::QUICK);
        AddTestCase(new TestP2PInstallation(), TestCase::QUICK);
        AddTestCase(new TestIpAssignment(), TestCase::QUICK);
        AddTestCase(new TestTcpCommunication(), TestCase::EXTENSIVE);
    }
};

// Test if nodes are correctly created
class TestNodeCreation : public TestCase
{
public:
    TestNodeCreation() : TestCase("Test Node Creation") {}

    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Should have exactly 2 nodes");
    }
};

// Test if Point-to-Point (P2P) devices are installed correctly
class TestP2PInstallation : public TestCase
{
public:
    TestP2PInstallation() : TestCase("Test P2P Installation") {}

    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Each node should have one P2P device installed");
    }
};

// Test if IP addresses are assigned correctly
class TestIpAssignment : public TestCase
{
public:
    TestIpAssignment() : TestCase("Test IP Assignment") {}

    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"),
                              "First node should have a valid IP");
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"),
                              "Second node should have a valid IP");
    }
};

// Test TCP communication between two nodes
class TestTcpCommunication : public TestCase
{
public:
    TestTcpCommunication() : TestCase("Test TCP Communication") {}

    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        uint16_t port = 5000;

        // Setup TCP server on Node 1
        PacketSinkHelper tcpServer("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Setup TCP client on Node 0
        OnOffHelper tcpClient("ns3::TcpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(1), port));
        tcpClient.SetAttribute("DataRate", StringValue("5Mbps"));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();

        Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
        NS_TEST_ASSERT_MSG_GT(sink->GetTotalRx(), 0, "Server should receive some data from the client");

        Simulator::Destroy();
    }
};

// Register the test suite
static P2PTcpTestSuite p2pTcpTestSuite;
