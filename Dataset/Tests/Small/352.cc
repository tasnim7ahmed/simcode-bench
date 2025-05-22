#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for PointToPointTcpExample
class PointToPointTcpTest : public TestCase
{
public:
    PointToPointTcpTest() : TestCase("Test for Point-to-Point TCP Server and Client") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
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
        nodes.Create(2); // Create 2 nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    // Test for Point-to-Point link setup
    void TestPointToPointLinkSetup()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify that the devices were installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install Point-to-Point devices");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the internet stack is installed
        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 0");

        ipv4 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 1");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        interfaces = address.Assign(nodes.Get(0)->GetDevice(0));
        interfaces.Add(address.Assign(nodes.Get(1)->GetDevice(0)));

        // Verify that IP addresses were assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    // Test for TCP server setup
    void TestTcpServerSetup()
    {
        uint16_t port = 8080;
        PacketSinkHelper tcpServer("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application was installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP Server not installed on Node 1");
    }

    // Test for TCP client setup
    void TestTcpClientSetup()
    {
        uint16_t port = 8080;
        OnOffHelper tcpClient("ns3::TcpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(1), port));
        tcpClient.SetAttribute("DataRate", StringValue("5Mbps"));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application was installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP Client not installed on Node 0");
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
static PointToPointTcpTest pointToPointTcpTestInstance;
