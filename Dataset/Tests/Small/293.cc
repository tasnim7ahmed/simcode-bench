#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

class TcpConnectionExampleTest : public TestCase
{
public:
    TcpConnectionExampleTest() : TestCase("Test TCP Connection Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestIpv4AddressAssignment();
        TestTcpServerInstallation();
        TestTcpClientInstallation();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of two nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Verify that two nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed, expected 2 nodes");
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

        // Verify that two devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-point link setup failed, expected 2 devices");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment to the devices
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Verify that IP addresses are assigned to both nodes
        Ipv4Address address1 = interfaces.GetAddress(0);
        Ipv4Address address2 = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_EQ(address1.IsValid(), true, "IP address assignment failed for node 0");
        NS_TEST_ASSERT_MSG_EQ(address2.IsValid(), true, "IP address assignment failed for node 1");
    }

    void TestTcpServerInstallation()
    {
        // Test TCP server installation on the second node
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Second node as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP server application installation failed");
    }

    void TestTcpClientInstallation()
    {
        // Test TCP client installation on the first node
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        TcpClientHelper tcpClient(interfaces.GetAddress(1), port); // Connecting to second node
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // First node as client
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        uint16_t port = 8080;
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Second node as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        TcpClientHelper tcpClient(interfaces.GetAddress(1), port); // Connecting to second node
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // First node as client
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Check if simulation completes successfully
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext()->GetTotalSimTime().GetSeconds() > 0, true, "Simulation did not run correctly");
    }
};

int main(int argc, char *argv[])
{
    // Run the unit tests
    Ptr<TcpConnectionExampleTest> test = CreateObject<TcpConnectionExampleTest>();
    test->Run();

    return 0;
}
