#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TcpClientServerExample
class TcpClientServerTest : public TestCase
{
public:
    TcpClientServerTest() : TestCase("Test for TCP Client-Server Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
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
        nodes.Create(2); // Create 2 nodes (server and client)
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    // Test for internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the internet stack was installed on both nodes
        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 0");
        ipv4 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on Node 1");
    }

    // Test for IP address assignment to nodes
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(nodes);

        // Verify that IP addresses have been assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    // Test for TCP server setup
    void TestTcpServerSetup()
    {
        uint16_t port = 9; // Port number for TCP connection
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application was installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP Server not installed on Node 0");
    }

    // Test for TCP client setup
    void TestTcpClientSetup()
    {
        uint16_t port = 9; // Port number for TCP connection
        TcpClientHelper tcpClient(interfaces.GetAddress(0), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(1));      // Send 1 packet
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval between packets
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));   // Packet size
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application was installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP Client not installed on Node 1");
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
static TcpClientServerTest tcpClientServerTestInstance;
