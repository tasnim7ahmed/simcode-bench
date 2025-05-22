#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for SimpleCsmaExample
class SimpleCsmaTest : public TestCase
{
public:
    SimpleCsmaTest() : TestCase("Test for Simple CSMA Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestCsmaSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpApplicationsSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nCsmaNodes = 4; // 3 clients + 1 server
    NodeContainer csmaNodes;
    NetDeviceContainer csmaDevices;
    Ipv4InterfaceContainer csmaInterfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        csmaNodes.Create(nCsmaNodes);
        NS_TEST_ASSERT_MSG_EQ(csmaNodes.GetN(), nCsmaNodes, "Failed to create the correct number of nodes");
    }

    // Test for CSMA setup
    void TestCsmaSetup()
    {
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        csmaDevices = csma.Install(csmaNodes);

        // Verify CSMA devices are installed
        NS_TEST_ASSERT_MSG_EQ(csmaDevices.GetN(), nCsmaNodes, "Failed to install CSMA devices on all nodes");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(csmaNodes);

        // Verify that the Internet stack is installed on the nodes
        Ptr<Ipv4> ipv4 = csmaNodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 0");

        ipv4 = csmaNodes.Get(nCsmaNodes - 1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on last node");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        csmaInterfaces = address.Assign(csmaDevices);

        // Verify that the IP addresses were assigned correctly
        for (uint32_t i = 0; i < nCsmaNodes; ++i)
        {
            NS_TEST_ASSERT_MSG_NE(csmaInterfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node " << i);
        }
    }

    // Test for UDP Echo server and client applications setup
    void TestUdpApplicationsSetup()
    {
        uint16_t port = 9;

        // Set up UDP Echo Server on the last node (server)
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(csmaNodes.Get(nCsmaNodes - 1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP Echo Clients on the first three nodes (clients)
        for (uint32_t i = 0; i < nCsmaNodes - 1; ++i)
        {
            UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(nCsmaNodes - 1), port);
            echoClient.SetAttribute("MaxPackets", UintegerValue(5));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
            echoClient.SetAttribute("PacketSize", UintegerValue(512));

            ApplicationContainer clientApp = echoClient.Install(csmaNodes.Get(i));
            clientApp.Start(Seconds(2.0));
            clientApp.Stop(Seconds(9.0));
        }

        // Verify application setup
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Echo server application not installed on server node");
        for (uint32_t i = 0; i < nCsmaNodes - 1; ++i)
        {
            ApplicationContainer clientApp = serverApp.Get(i);
            NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Echo client application not installed on client node " << i);
        }
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
static SimpleCsmaTest simpleCsmaTestInstance;
