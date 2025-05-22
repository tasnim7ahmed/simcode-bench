#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for PointToPointTcpExample
class PointToPointTcpTest : public TestCase
{
public:
    PointToPointTcpTest() : TestCase("Test for PointToPointTcpExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointDeviceInstallation();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestTcpApplicationSetup();
        TestFlowMonitorSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 2;
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    Ptr<FlowMonitor> monitor;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create the correct number of nodes");
    }

    // Test for PointToPoint device installation
    void TestPointToPointDeviceInstallation()
    {
        // Set up point-to-point link with 10Mbps link speed and 5ms delay
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        // Install point-to-point devices
        devices = pointToPoint.Install(nodes);

        // Verify devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install point-to-point devices on all nodes");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack is installed on both nodes
        NS_TEST_ASSERT_MSG_NE(nodes.Get(0)->GetObject<Ipv4>(), nullptr, "IPv4 stack not installed on node 0");
        NS_TEST_ASSERT_MSG_NE(nodes.Get(1)->GetObject<Ipv4>(), nullptr, "IPv4 stack not installed on node 1");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned to both devices
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node 0");
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node 1");
    }

    // Test for TCP application setup (client and server)
    void TestTcpApplicationSetup()
    {
        uint16_t port = 9;

        // Set up TCP client application on node 0 (sender)
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the TCP client application is installed on node 0
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on node 0");

        // Set up TCP server application on node 1 (receiver)
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the TCP server application is installed on node 1
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on node 1");
    }

    // Test for Flow Monitor setup
    void TestFlowMonitorSetup()
    {
        // Set up flow monitor to capture throughput
        FlowMonitorHelper flowmon;
        monitor = flowmon.InstallAll();

        // Verify that the flow monitor is set up correctly
        NS_TEST_ASSERT_MSG_NE(monitor, nullptr, "Flow monitor not installed correctly");
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
static PointToPointTcpTest pointToPointTcpTestInstance;
