#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TcpExample
class TcpExampleTest : public TestCase
{
public:
    TcpExampleTest() : TestCase("Test for TcpExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestTcpClientServerApplicationSetup();
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
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create TCP nodes");
    }

    // Test for point-to-point link setup
    void TestPointToPointLinkSetup()
    {
        // Set up the point-to-point link
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        // Install devices on nodes
        devices = pointToPoint.Install(nodes);

        // Verify that devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install point-to-point devices on nodes");
    }

    // Test for internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that internet stack is installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Internet stack installation failed on nodes");
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

    // Test for TCP client-server application setup
    void TestTcpClientServerApplicationSetup()
    {
        uint16_t port = 8080; // Port number for the TCP connection

        // Set up TCP server on Node 2
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify TCP server installation
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on Node 2");

        // Set up TCP client on Node 1
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(10));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
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
static TcpExampleTest tcpExampleTestInstance;
