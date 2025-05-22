#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TcpServerClientExample
class TcpServerClientTest : public TestCase
{
public:
    TcpServerClientTest() : TestCase("Test for TCP Server-Client Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointLinkConfiguration();
        TestDeviceInstallation();
        TestIpAddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(2); // Create 2 nodes: 1 client, 1 server
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    // Test for point-to-point link setup
    void TestPointToPointLinkConfiguration()
    {
        // Set up point-to-point link with specific parameters
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        devices = pointToPoint.Install(nodes);

        // Verify devices installation
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install point-to-point devices on all nodes");
    }

    // Test for device installation
    void TestDeviceInstallation()
    {
        // Verify that the devices are installed properly on both nodes
        for (uint32_t i = 0; i < devices.GetN(); ++i)
        {
            Ptr<NetDevice> device = devices.Get(i);
            NS_TEST_ASSERT_MSG_NOT_NULL(device, "Device not installed on node");
        }
    }

    // Test for IP address assignment to devices
    void TestIpAddressAssignment()
    {
        // Assign IP addresses to the devices
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses have been correctly assigned
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    // Test for TCP server application installation
    void TestTcpServerSetup()
    {
        uint16_t port = 9; // Port for server
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Install on server node 1
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify server application installation
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP Server application not installed on server node");
    }

    // Test for TCP client application installation
    void TestTcpClientSetup()
    {
        uint16_t port = 9; // Port for client
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port); // Server address and port 9
        tcpClient.SetAttribute("MaxPackets", UintegerValue(10));   // Send 10 packets
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 second
        tcpClient.SetAttribute("PacketSize", UintegerValue(512));   // 512 bytes packet
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Install on client node 0
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify client application installation
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP Client application not installed on client node");
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
static TcpServerClientTest tcpServerClientTestInstance;
