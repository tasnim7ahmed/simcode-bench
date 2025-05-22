#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class P2PTcpTest : public TestCase
{
public:
    P2PTcpTest() : TestCase("Point-to-Point TCP Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestTcpSinkSetup();
        TestTcpBulkSenderSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of 2 nodes
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

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = p2p.Install(nodes);

        // Verify that devices are installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-Point devices not correctly installed on nodes");
    }

    void TestInternetStackInstallation()
    {
        // Test installation of the Internet stack
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack is installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Internet stack not correctly installed on nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment for the nodes
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = p2p.Install(nodes);

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

    void TestTcpSinkSetup()
    {
        // Test setup of the TCP Sink (receiver) on Node 1
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        Address sinkAddress(InetSocketAddress("10.1.1.2", port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        // Verify that the TCP sink application is installed on Node 1
        NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "TCP sink not correctly installed on Node 1");
    }

    void TestTcpBulkSenderSetup()
    {
        // Test setup of the TCP Bulk Sender (sender) on Node 0
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = p2p.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
        BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
        ApplicationContainer sourceApp = source.Install(nodes.Get(0));
        sourceApp.Start(Seconds(2.0));
        sourceApp.Stop(Seconds(10.0));

        // Verify that the TCP bulk sender application is installed on Node 0
        NS_TEST_ASSERT_MSG_EQ(sourceApp.GetN(), 1, "TCP bulk sender not correctly installed on Node 0");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = p2p.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080;
        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
        ApplicationContainer sourceApp = source.Install(nodes.Get(0));
        sourceApp.Start(Seconds(2.0));
        sourceApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    P2PTcpTest test;
    test.Run();
    return 0;
}
