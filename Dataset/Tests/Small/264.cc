#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class CsmaNetworkTestSuite : public TestCase
{
public:
    CsmaNetworkTestSuite() : TestCase("Test CSMA Network Setup for Nodes") {}
    virtual ~CsmaNetworkTestSuite() {}

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
    // Test node creation (ensure nodes are created successfully)
    void TestNodeCreation()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4); // Create 4 CSMA nodes

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(csmaNodes.GetN(), 4, "Failed to create the expected number of CSMA nodes.");
    }

    // Test CSMA device installation (verify CSMA devices are installed correctly)
    void TestCsmaDeviceInstallation()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

        // Verify that CSMA devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(csmaDevices.GetN(), 4, "Failed to install CSMA devices on nodes.");
    }

    // Test IP address assignment (ensure that IP addresses are assigned correctly)
    void TestIpAddressAssignment()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

        InternetStackHelper stack;
        stack.Install(csmaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

        // Verify that the IP addresses are assigned correctly to the devices
        for (uint32_t i = 0; i < csmaNodes.GetN(); ++i)
        {
            Ipv4Address ipAddr = csmaInterfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node.");
        }
    }

    // Test UDP Echo Server setup (verify that the server application is installed correctly)
    void TestUdpEchoServerSetup()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

        InternetStackHelper stack;
        stack.Install(csmaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(3)); // Install server on last node
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on the last node.");
    }

    // Test UDP Echo Client setup (verify that the client application is installed correctly)
    void TestUdpEchoClientSetup()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

        InternetStackHelper stack;
        stack.Install(csmaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

        UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(3), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(csmaNodes.Get(0)); // Install client on first node
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the client application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on the first node.");
    }

    // Test data transmission (verify that the client-server communication is successful)
    void TestDataTransmission()
    {
        NodeContainer csmaNodes;
        csmaNodes.Create(4);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

        InternetStackHelper stack;
        stack.Install(csmaNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(3));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(3), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(csmaNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Check if the number of packets transmitted matches the expected value
        // You can use tracing or checking the simulation log for packet counts here
    }
};

// Instantiate the test suite
static CsmaNetworkTestSuite csmaNetworkTestSuite;
