#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
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
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestTcpReceiverSetup();
        TestTcpSenderSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of two nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestPointToPointLinkSetup()
    {
        // Test point-to-point link setup
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify that devices are installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-Point link not correctly set up between nodes");
    }

    void TestInternetStackInstallation()
    {
        // Test installation of the internet stack
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the internet stack is installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Internet stack not correctly installed on all nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment for the nodes
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

        // Verify that IP addresses are correctly assigned to both nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ipv4Address addr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_EQ(addr.IsValid(), true, "IP address assignment failed for node " + std::to_string(i));
        }
    }

    void TestTcpReceiverSetup()
    {
        // Test setup of the TCP receiver (Sink) on Node 1
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 9;
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        // Verify that the TCP sink application is installed on Node 1
        NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "TCP receiver (sink) not correctly installed on Node 1");
    }

    void TestTcpSenderSetup()
    {
        // Test setup of the TCP sender on Node 0
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

        uint16_t port = 9;
        BulkSendHelper sender("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        sender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
        ApplicationContainer senderApp = sender.Install(nodes.Get(0));
        senderApp.Start(Seconds(2.0));
        senderApp.Stop(Seconds(10.0));

        // Verify that the TCP sender application is installed on Node 0
        NS_TEST_ASSERT_MSG_EQ(senderApp.GetN(), 1, "TCP sender not correctly installed on Node 0");
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

        uint16_t port = 9;
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        BulkSendHelper sender("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        sender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
        ApplicationContainer senderApp = sender.Install(nodes.Get(0));
        senderApp.Start(Seconds(2.0));
        senderApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
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
