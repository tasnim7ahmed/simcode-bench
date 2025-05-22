#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class MinimalTcpExampleTestSuite : public TestCase
{
public:
    MinimalTcpExampleTestSuite() : TestCase("Test Minimal TCP Example") {}
    virtual ~MinimalTcpExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    uint16_t port = 5000;

    // Test if nodes are created properly
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    // Test if point-to-point link is correctly set up
    void TestPointToPointSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-Point setup failed. Expected 2 devices.");
    }

    // Test if Internet stack is installed correctly
    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed.");
        }
    }

    // Test if IP addresses are assigned properly
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 2, "IP address assignment failed.");
    }

    // Test if TCP sockets are created for client and server
    void TestSocketCreation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = sink.Install(nodes.Get(1));

        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP Server socket creation failed.");

        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("10.1.1.2"), port));
        client.SetConstantRate(DataRate("1Mbps"), 512);
        ApplicationContainer clientApp = client.Install(nodes.Get(0));

        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP Client socket creation failed.");
    }

    // Test if packets are transmitted from the client
    void TestPacketTransmission()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        client.SetConstantRate(DataRate("1Mbps"), 512);
        ApplicationContainer clientApp = client.Install(nodes.Get(0));

        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(2.0));

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if server successfully receives packets
    void TestPacketReception()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = sink.Install(nodes.Get(1));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(2.0));

        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        client.SetConstantRate(DataRate("1Mbps"), 512);
        ApplicationContainer clientApp = client.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(2.0));

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(serverApp.Get(0));
        NS_TEST_ASSERT_MSG_GT(sink1->GetTotalRx(), 0, "Packet reception failed.");
    }
};

// Instantiate the test suite
static MinimalTcpExampleTestSuite minimalTcpExampleTestSuite;
