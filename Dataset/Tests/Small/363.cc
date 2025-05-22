#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ethernet-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for EthernetTcpExample
class EthernetTcpTest : public TestCase
{
public:
    EthernetTcpTest() : TestCase("Test for EthernetTcpExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestEthernetLinkSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
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

    // Test for Ethernet point-to-point link setup
    void TestEthernetLinkSetup()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        devices = pointToPoint.Install(nodes);

        // Verify devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install Ethernet devices on nodes");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack is installed on the nodes
        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 0");

        ipv4 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 1");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that the IP addresses are assigned correctly
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node 0");
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node 1");
    }

    // Test for application setup (TCP server and client)
    void TestApplicationSetup()
    {
        uint16_t port = 9;

        // Set up TCP server on node 1
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up TCP client on node 0
        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));

        // Verify that the server and client applications are set up
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on node 1");
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
static EthernetTcpTest ethernetTcpTestInstance;
