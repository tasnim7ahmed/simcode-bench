#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TcpConnectionExample
class TcpConnectionExampleTest : public TestCase
{
public:
    TcpConnectionExampleTest() : TestCase("Test for TcpConnectionExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestLinkSetup();
        TestIpv4AddressAssignment();
        TestTcpApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 2;
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);

        // Check that nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create nodes");
    }

    // Test for link setup (point-to-point)
    void TestLinkSetup()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install devices on nodes
        devices = pointToPoint.Install(nodes);

        // Verify devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install devices on nodes");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");

        // Assign IP addresses to devices
        interfaces = ipv4.Assign(devices);

        // Verify that IP addresses are assigned
        Ipv4Address node1Ip = interfaces.GetAddress(0);
        Ipv4Address node2Ip = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
    }

    // Test for TCP application setup (server and client)
    void TestTcpApplicationSetup()
    {
        uint16_t port = 9; // Arbitrary port

        // Set up TCP server on Node 2
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify TCP server installation
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on Node 2");

        // Set up TCP client on Node 1
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify TCP client installation
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on Node 1");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static TcpConnectionExampleTest tcpConnectionExampleTestInstance;
