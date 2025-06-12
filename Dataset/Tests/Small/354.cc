#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleP2PExample
class SimpleP2PTest : public TestCase
{
public:
    SimpleP2PTest() : TestCase("Test for Simple Point-to-Point Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestP2PLinkSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create the correct number of nodes");
    }

    // Test for point-to-point link setup
    void TestP2PLinkSetup()
    {
        // Set up the point-to-point link
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install the devices on the nodes
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify that devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-point devices not installed correctly");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack is installed
        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 0");

        ipv4 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 1");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(nodes.Get(0)->GetDevice(0));
        address.Assign(nodes.Get(1)->GetDevice(0));

        // Verify that the IP addresses were assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node 0");
        }
    }

    // Test for TCP server setup
    void TestTcpServerSetup()
    {
        uint16_t port = 50000;
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = sinkHelper.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application was installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP Server not installed on node 1");
    }

    // Test for TCP client setup
    void TestTcpClientSetup()
    {
        uint16_t port = 50000;
        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
        ApplicationContainer clientApp = sourceHelper.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application was installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP Client not installed on node 0");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static SimpleP2PTest simpleP2PTestInstance;
