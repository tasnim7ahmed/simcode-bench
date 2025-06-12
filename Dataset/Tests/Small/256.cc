#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class CsmaTestSuite : public TestCase
{
public:
    CsmaTestSuite() : TestCase("Test CSMA Echo Application Setup") {}
    virtual ~CsmaTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestCsmaSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpApplication();
    }

private:
    // Test the creation of CSMA nodes
    void TestNodeCreation()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4); // Create 4 nodes

        // Ensure that 4 nodes are created
        NS_TEST_ASSERT_MSG_EQ(csmaNodes.GetN(), 4, "Failed to create the correct number of nodes.");
    }

    // Test CSMA setup (channel and devices installation)
    void TestCsmaSetup()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4); // Create 4 nodes

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices;
        csmaDevices = csma.Install(csmaNodes);

        // Ensure devices are installed
        NS_TEST_ASSERT_MSG_EQ(csmaDevices.GetN(), 4, "Failed to install CSMA devices on the nodes.");
    }

    // Test the installation of the internet stack on the nodes
    void TestInternetStackInstallation()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4); // Create 4 nodes

        InternetStackHelper stack;
        stack.Install(csmaNodes);

        // Verify that internet stack is installed
        Ptr<Ipv4> ipv4Node = csmaNodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NE(ipv4Node, nullptr, "Internet stack installation failed on node.");
    }

    // Test IP address assignment
    void TestIpAddressAssignment()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4); // Create 4 nodes

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices;
        csmaDevices = csma.Install(csmaNodes);

        InternetStackHelper stack;
        stack.Install(csmaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);

        // Verify that IP addresses are assigned correctly to each node
        Ipv4Address ipNode1 = interfaces.GetAddress(0);
        Ipv4Address ipNode2 = interfaces.GetAddress(1);
        Ipv4Address ipNode3 = interfaces.GetAddress(2);
        Ipv4Address ipNode4 = interfaces.GetAddress(3);

        NS_TEST_ASSERT_MSG_NE(ipNode1, Ipv4Address("0.0.0.0"), "Failed to assign IP address to Node 1.");
        NS_TEST_ASSERT_MSG_NE(ipNode2, Ipv4Address("0.0.0.0"), "Failed to assign IP address to Node 2.");
        NS_TEST_ASSERT_MSG_NE(ipNode3, Ipv4Address("0.0.0.0"), "Failed to assign IP address to Node 3.");
        NS_TEST_ASSERT_MSG_NE(ipNode4, Ipv4Address("0.0.0.0"), "Failed to assign IP address to Node 4.");
    }

    // Test the UDP Echo Server and Client application setup
    void TestUdpApplication()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4); // Create 4 nodes

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices;
        csmaDevices = csma.Install(csmaNodes);

        InternetStackHelper stack;
        stack.Install(csmaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(3)); // Server on last node
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(2));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(csmaNodes.Get(0)); // Client on first node
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Ensure that the server and client applications are installed and running
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on the last node.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on the first node.");
    }
};

// Instantiate the test suite
static CsmaTestSuite csmaTestSuite;
