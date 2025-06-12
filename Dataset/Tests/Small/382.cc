#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ethernet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for CSMA Network Example
class CsmExampleTest : public TestCase
{
public:
    CsmExampleTest() : TestCase("Test for CSMA Network Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestCsmaLinkSetup();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpClientServerApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 3;
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);

        // Check that nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create CSMA nodes");
    }

    // Test for CSMA link setup
    void TestCsmaLinkSetup()
    {
        // Set up CSMA channel
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install devices on nodes
        devices = csma.Install(nodes);

        // Verify that devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install CSMA devices on nodes");
    }

    // Test for internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the internet stack is installed on all nodes
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
        Ipv4Address node3Ip = interfaces.GetAddress(2);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
        NS_TEST_ASSERT_MSG_NE(node3Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 3");
    }

    // Test for UDP client-server application setup
    void TestUdpClientServerApplicationSetup()
    {
        uint16_t port = 9; // Port number for the UDP connection

        // Set up UDP server on Node 3
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(2));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify UDP server installation
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on Node 3");

        // Set up UDP client on Node 1
        UdpClientHelper udpClient(interfaces.GetAddress(2), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify UDP client installation
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on Node 1");
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
static CsmExampleTest csmExampleTestInstance;
