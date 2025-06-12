#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpEchoTestSuite : public TestCase
{
public:
    UdpEchoTestSuite() : TestCase("Test UDP Echo Client-Server Setup") {}
    virtual ~UdpEchoTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLink();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpApplicationSetup();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation()
    {
        uint32_t numNodes = 2;

        NodeContainer nodes;
        nodes.Create(numNodes);

        // Ensure that the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), numNodes, "Node creation failed: Expected " << numNodes << " nodes.");
    }

    // Test the point-to-point link setup
    void TestPointToPointLink()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify that devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-Point link failed: Expected 2 devices.");
    }

    // Test the installation of the Internet stack on nodes
    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        // Ensure that the Internet stack is installed correctly
        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on Node 0.");
    }

    // Test the IP address assignment to the nodes
    void TestIpAddressAssignment()
    {
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

        // Verify that IP addresses are assigned correctly
        Ipv4Address ip0 = interfaces.GetAddress(0);
        Ipv4Address ip1 = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(ip0, Ipv4Address("0.0.0.0"), "Failed to assign IP address to Node 0.");
        NS_TEST_ASSERT_MSG_NE(ip1, Ipv4Address("0.0.0.0"), "Failed to assign IP address to Node 1.");
    }

    // Test the UDP application setup
    void TestUdpApplicationSetup()
    {
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

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the server and client applications are installed
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client.");
    }
};

// Instantiate the test suite
static UdpEchoTestSuite udpEchoTestSuite;
