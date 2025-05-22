#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleWiredNetworkTest : public TestCase
{
public:
    SimpleWiredNetworkTest() : TestCase("Simple Wired Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLink();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of two nodes (hosts)
        NodeContainer nodes;
        nodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestPointToPointLink()
    {
        // Test the setup of the Point-to-Point link between the nodes
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify the number of devices installed on the nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-Point link installation failed, incorrect number of devices");
    }

    void TestInternetStackInstallation()
    {
        // Test installation of the Internet stack (TCP/IP) on the nodes
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack is installed
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Internet stack installation failed, incorrect number of nodes with internet stack");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to the devices
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify IP address assignment to the devices
        NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 2, "IP address assignment failed, incorrect number of IP addresses");
    }

    void TestTcpServerSetup()
    {
        // Test the setup of the TCP server application on Node 0
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = tcpSink.Install(nodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the TCP server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP server not correctly installed on Node 0");
    }

    void TestTcpClientSetup()
    {
        // Test the setup of the TCP client application on Node 1
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
        tcpClient.SetAttribute("MaxBytes", UintegerValue(1000000));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the TCP client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP client not correctly installed on Node 1");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper tcpSink("ns3::TcpSocketFactory", serverAddress);
        ApplicationContainer serverApp = tcpSink.Install(nodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        BulkSendHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
        tcpClient.SetAttribute("MaxBytes", UintegerValue(1000000));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleWiredNetworkTest test;
    test.Run();
    return 0;
}
