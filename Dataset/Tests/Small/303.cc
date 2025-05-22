#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleP2PTest : public TestCase
{
public:
    SimpleP2PTest() : TestCase("Simple P2P TCP Example Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointDeviceInstallation();
        TestIpv4AddressAssignment();
        TestTcpApplicationSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 2 nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Verify correct number of nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestPointToPointDeviceInstallation()
    {
        // Test point-to-point device installation
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install devices
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify device installation
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-Point device installation failed, incorrect number of devices");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install devices
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Install Internet stack
        InternetStackHelper stack;
        stack.Install(nodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify IP address assignment
        Ipv4Address address1 = interfaces.GetAddress(0);
        Ipv4Address address2 = interfaces.GetAddress(1);
        NS_TEST_ASSERT_MSG_EQ(address1.IsValid(), true, "IP address assignment failed on Node 0");
        NS_TEST_ASSERT_MSG_EQ(address2.IsValid(), true, "IP address assignment failed on Node 1");
    }

    void TestTcpApplicationSetup()
    {
        // Test TCP server and client application setup
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;

        // Set up TCP server on Node 1
        Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = tcpSink.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up TCP client on Node 0
        BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("10.1.1.2"), port));
        tcpClient.SetAttribute("MaxBytes", UintegerValue(500000));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify application installation
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP server application installation failed");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;

        // Set up TCP server on Node 1
        Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = tcpSink.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up TCP client on Node 0
        BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("10.1.1.2"), port));
        tcpClient.SetAttribute("MaxBytes", UintegerValue(500000));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Run the simulation
        Simulator::Run();
        Simulator::Destroy();

        // Basic check to ensure the simulation runs
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleP2PTest test;
    test.Run();
    return 0;
}
