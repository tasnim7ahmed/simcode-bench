#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Tcp Example
class TcpExampleTest : public TestCase
{
public:
    TcpExampleTest() : TestCase("Test for TCP Example") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 2;
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        nodes.Create(nNodes);

        // Verify nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), nNodes, "Failed to create the expected number of nodes");
    }

    // Test for Point-to-Point link setup
    void TestPointToPointLinkSetup()
    {
        // Set up point-to-point link with delay and packet loss
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));
        pointToPoint.SetChannelAttribute("Loss", StringValue("0.1"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), nNodes, "Failed to install devices on nodes");

        // Verify the point-to-point channel parameters are set correctly
        Ptr<NetDevice> device0 = devices.Get(0);
        Ptr<PointToPointNetDevice> p2pDevice0 = DynamicCast<PointToPointNetDevice>(device0);
        NS_TEST_ASSERT_MSG_EQ(p2pDevice0->GetChannel()->GetAttribute("Delay").Get<std::string>(), "10ms", "Incorrect channel delay");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that Internet stack is installed on all nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Failed to install Internet stack on node");
        }
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");

        // Assign IP addresses to nodes
        interfaces = ipv4.Assign(nodes);

        // Verify that IP addresses are assigned to nodes
        Ipv4Address node1Ip = interfaces.GetAddress(0);
        Ipv4Address node2Ip = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(node1Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 1");
        NS_TEST_ASSERT_MSG_NE(node2Ip, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node 2");
    }

    // Test for TCP server setup
    void TestTcpServerSetup()
    {
        uint16_t port = 8080;
        TcpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that server application was installed successfully
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install TCP server on Node 1");
    }

    // Test for TCP client setup
    void TestTcpClientSetup()
    {
        uint16_t port = 8080;
        TcpClientHelper client(interfaces.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = client.Install(nodes.Get(0));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that client application was installed successfully
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install TCP client on Node 0");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error during execution");
    }
};

// Register the test
static TcpExampleTest tcpExampleTestInstance;
