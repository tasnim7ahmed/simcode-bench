#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for PointToPointUdpExample
class PointToPointUdpTest : public TestCase
{
public:
    PointToPointUdpTest() : TestCase("Test for PointToPointUdpExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointDeviceInstallation();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpApplicationSetup();
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

    // Test for PointToPoint device installation
    void TestPointToPointDeviceInstallation()
    {
        // Set up point-to-point link with 5Mbps link speed and 2ms delay
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

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

    // Test for UDP application setup (client and server)
    void TestUdpApplicationSetup()
    {
        uint16_t port = 9;

        // Set up UDP client application on node 0 (sender)
        UdpClientHelper udpClient(interfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application is installed on node 0
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on node 0");

        // Set up UDP server application on node 1 (receiver)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application is installed on node 1
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on node 1");
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
static PointToPointUdpTest pointToPointUdpTestInstance;
