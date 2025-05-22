#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Ipv6PingExample
class Ipv6PingTest : public TestCase
{
public:
    Ipv6PingTest() : TestCase("Test for Ipv6PingExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestIpv6LinkSetup();
        TestIpv6StackInstallation();
        TestIpv6AddressAssignment();
        TestPingApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 2;
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv6InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);
        
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create the correct number of nodes");
    }

    // Test for IPv6 Point-to-Point link setup
    void TestIpv6LinkSetup()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

        devices = pointToPoint.Install(nodes);

        // Verify devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install IPv6 devices on nodes");
    }

    // Test for IPv6 stack installation
    void TestIpv6StackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack (IPv6) is installed on the nodes
        Ptr<Ipv6> ipv6 = nodes.Get(0)->GetObject<Ipv6>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv6, "IPv6 stack not installed on node 0");

        ipv6 = nodes.Get(1)->GetObject<Ipv6>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv6, "IPv6 stack not installed on node 1");
    }

    // Test for IPv6 address assignment
    void TestIpv6AddressAssignment()
    {
        Ipv6AddressHelper address;
        address.SetBase(Ipv6Address("2001:1::"), Ipv6Mask("/64"));
        interfaces = address.Assign(devices);

        // Verify that the IPv6 addresses are assigned correctly
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv6Address("::"), "Invalid IPv6 address assigned to node 0");
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv6Address("::"), "Invalid IPv6 address assigned to node 1");
    }

    // Test for ping application setup
    void TestPingApplicationSetup()
    {
        // Set up ICMP Echo Request (ping) from node 0 to node 1
        V4PingHelper ping(interfaces.GetAddress(1));
        ApplicationContainer pingApp = ping.Install(nodes.Get(0));
        pingApp.Start(Seconds(2.0));
        pingApp.Stop(Seconds(10.0));

        // Verify that the ping application is set up correctly
        NS_TEST_ASSERT_MSG_GT(pingApp.GetN(), 0, "Ping application not installed on node 0");
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
static Ipv6PingTest ipv6PingTestInstance;
