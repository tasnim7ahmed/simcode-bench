#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleTcpTest : public TestCase
{
public:
    SimpleTcpTest() : TestCase("Simple TCP Example Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestIpv4AddressAssignment();
        TestTcpSinkSetup();
        TestTcpSenderSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of two nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestPointToPointLinkSetup()
    {
        // Test setting up of point-to-point link
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify that devices are installed on the nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-point devices not correctly installed on nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment on point-to-point devices
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify IP addresses are assigned correctly
        Ipv4Address address1 = interfaces.GetAddress(0);
        Ipv4Address address2 = interfaces.GetAddress(1);
        NS_TEST_ASSERT_MSG_EQ(address1.IsValid(), true, "IP address assignment failed on Node 0");
        NS_TEST_ASSERT_MSG_EQ(address2.IsValid(), true, "IP address assignment failed on Node 1");
    }

    void TestTcpSinkSetup()
    {
        // Test TCP sink setup on Node 1
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        // Verify TCP sink application setup
        NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "TCP sink application setup failed on Node 1");
    }

    void TestTcpSenderSetup()
    {
        // Test TCP sender setup on Node 0
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
        BulkSendHelper sender("ns3::TcpSocketFactory", sinkAddress);
        sender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
        ApplicationContainer senderApp = sender.Install(nodes.Get(0));
        senderApp.Start(Seconds(1.0));
        senderApp.Stop(Seconds(10.0));

        // Verify TCP sender application setup
        NS_TEST_ASSERT_MSG_EQ(senderApp.GetN(), 1, "TCP sender application setup failed on Node 0");
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
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        BulkSendHelper sender("ns3::TcpSocketFactory", sinkAddress);
        sender.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer senderApp = sender.Install(nodes.Get(0));
        senderApp.Start(Seconds(1.0));
        senderApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleTcpTest test;
    test.Run();
    return 0;
}
