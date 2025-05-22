#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TCP Client-Server Communication
class TcpClientServerExampleTest : public TestCase
{
public:
    TcpClientServerExampleTest() : TestCase("Test for TCP Client-Server Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointLink();
        TestIpAddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    void TestNodeCreation()
    {
        nodes.Create(2); // Create two nodes: one for the client and one for the server
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    void TestPointToPointLink()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        devices = pointToPoint.Install(nodes);
        
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install point-to-point devices on nodes");
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned correctly to both nodes
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    void TestTcpServerSetup()
    {
        uint16_t port = 9; // Port number
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Install on server node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on server node");
    }

    void TestTcpClientSetup()
    {
        uint16_t port = 9; // Port number
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port); // Connect to server node
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));    // Send 100 packets
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 seconds
        tcpClient.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes packets
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Install on client node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on client node");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static TcpClientServerExampleTest tcpClientServerExampleTestInstance;
