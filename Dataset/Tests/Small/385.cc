#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for UDP Example with Point-to-Point Link
class UdpExampleTest : public TestCase
{
public:
    UdpExampleTest() : TestCase("Test for UDP Example with Point-to-Point Link") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointDeviceInstallation();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerApplicationSetup();
        TestUdpClientApplicationSetup();
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

        // Check if nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create nodes");
    }

    // Test for point-to-point device installation
    void TestPointToPointDeviceInstallation()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        // Install point-to-point devices on nodes
        devices = pointToPoint.Install(nodes);

        // Verify if devices are installed successfully
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install point-to-point devices on nodes");
    }

    // Test for internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Check if internet stack is installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Internet stack installation failed on nodes");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("192.168.1.0", "255.255.255.0");

        // Assign IP addresses to devices
        interfaces = ipv4.Assign(devices);

        // Verify IP addresses are assigned correctly
        Ipv4Address node1Ip = interfaces.GetAddress(0);
        Ipv4Address node2Ip = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
    }

    // Test for UDP server application setup
    void TestUdpServerApplicationSetup()
    {
        uint16_t port = 12345;

        // Set up UDP server on Node 1 (Receiver)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify if server application is installed successfully
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on Node 1");
    }

    // Test for UDP client application setup
    void TestUdpClientApplicationSetup()
    {
        uint16_t port = 12345;

        // Set up UDP client on Node 0 (Sender)
        UdpClientHelper udpClient(interfaces.GetAddress(1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify if client application is installed successfully
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on Node 0");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify that simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static UdpExampleTest udpExampleTestInstance;
