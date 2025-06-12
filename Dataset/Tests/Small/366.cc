#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TcpNewRenoExample
class TcpNewRenoTest : public TestCase
{
public:
    TcpNewRenoTest() : TestCase("Test for TcpNewRenoExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
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
        
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create the correct number of nodes");
    }

    // Test for Point-to-Point link setup
    void TestPointToPointLinkSetup()
    {
        // Set up Point-to-Point link
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

        devices = pointToPoint.Install(nodes);

        // Verify devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install Point-to-Point devices on nodes");
    }

    // Test for IPv4 address assignment
    void TestIpv4AddressAssignment()
    {
        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned correctly
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IPv4 address assigned to node 0");
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Invalid IPv4 address assigned to node 1");
    }

    // Test for TCP application setup (client and server)
    void TestTcpApplicationSetup()
    {
        uint16_t port = 9;

        // Set up TCP server application on node 1 (server)
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify the TCP server application is set up correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on node 1");

        // Set up TCP client application on node 0 (client)
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));

        // Verify the TCP client application is set up correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on node 0");
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
static TcpNewRenoTest tcpNewRenoTestInstance;
