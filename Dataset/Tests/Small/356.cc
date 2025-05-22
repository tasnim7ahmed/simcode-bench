#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleP2PTCPExample
class SimpleP2PTest : public TestCase
{
public:
    SimpleP2PTest() : TestCase("Test for Simple P2P TCP Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestP2PLinkSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestBulkSendApplicationSetup();
        TestPacketSinkApplicationSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create the correct number of nodes");
    }

    // Test for Point-to-Point link setup
    void TestP2PLinkSetup()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify that the devices were installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install devices on nodes");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper internet;
        internet.Install(nodes);

        // Verify that the Internet stack is installed
        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 0");

        ipv4 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 1");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(nodes.Get(0)->GetDevice(0));
        address.Assign(nodes.Get(1)->GetDevice(0));

        // Verify that the IP addresses were assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned");
        }
    }

    // Test for BulkSend application setup
    void TestBulkSendApplicationSetup()
    {
        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
        ApplicationContainer sourceApp = source.Install(nodes.Get(0));
        sourceApp.Start(Seconds(1.0));
        sourceApp.Stop(Seconds(10.0));

        // Verify that the BulkSend application is installed correctly
        NS_TEST_ASSERT_MSG_GT(sourceApp.GetN(), 0, "BulkSend application not installed on node 0");
    }

    // Test for PacketSink application setup
    void TestPacketSinkApplicationSetup()
    {
        uint16_t port = 9;
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        // Verify that the PacketSink application is installed correctly
        NS_TEST_ASSERT_MSG_GT(sinkApp.GetN(), 0, "PacketSink application not installed on node 1");
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
static SimpleP2PTest simpleP2PTestInstance;
