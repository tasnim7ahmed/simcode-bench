#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class CsmaTestSuite : public TestCase
{
public:
    CsmaTestSuite() : TestCase("Test CSMA with UDP Echo Application") {}
    virtual ~CsmaTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestCsmaDeviceInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that four nodes are created correctly)
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(4);

        // Verify that four nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Failed to create the expected number of nodes.");
    }

    // Test CSMA device installation (verify that CSMA devices are installed correctly on the nodes)
    void TestCsmaDeviceInstallation()
    {
        NodeContainer nodes;
        nodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer devices = csma.Install(nodes);

        // Verify that CSMA devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 4, "Failed to install CSMA devices on nodes.");
    }

    // Test IP address assignment (verify that IP addresses are correctly assigned to devices)
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer devices = csma.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned correctly
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ipv4Address ipAddr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node.");
        }
    }

    // Test UDP Echo Server setup (verify that the UDP Echo Server is set up correctly on the last node)
    void TestUdpEchoServerSetup()
    {
        NodeContainer nodes;
        nodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer devices = csma.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Server is installed on the last node
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on the last node.");
    }

    // Test UDP Echo Client setup (verify that the UDP Echo Client is set up correctly on the first node)
    void TestUdpEchoClientSetup()
    {
        NodeContainer nodes;
        nodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer devices = csma.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Client is installed on the first node
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on the first node.");
    }

    // Test data transmission (verify that data is transmitted between the UDP Echo Client and Server)
    void TestDataTransmission()
    {
        NodeContainer nodes;
        nodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer devices = csma.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Additional checks can be added to verify that the client-server communication was successful.
    }
};

// Register the test suite
TestSuite csmaTestSuite("CsmaTestSuite", SYSTEM);
csmaTestSuite.AddTestCase(new CsmaTestSuite());
