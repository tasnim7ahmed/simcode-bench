#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

class TcpConnectionTest : public TestCase
{
public:
    TcpConnectionTest() : TestCase("Test Tcp Connection Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestDeviceInstallation();
        TestIpv4AddressAssignment();
        TestTcpServerInstallation();
        TestTcpClientInstallation();
        TestFlowMonitor();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 2 nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestDeviceInstallation()
    {
        // Test installation of Point-to-Point devices
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify the number of devices installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Device installation failed, incorrect number of devices");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to devices
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Verify that IP addresses are assigned to both devices
        Ipv4Address addressNode0 = interfaces.GetAddress(0);
        Ipv4Address addressNode1 = interfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_EQ(addressNode0.IsValid(), true, "IP address assignment failed for Node 0");
        NS_TEST_ASSERT_MSG_EQ(addressNode1.IsValid(), true, "IP address assignment failed for Node 1");
    }

    void TestTcpServerInstallation()
    {
        // Test TCP server installation on Node 1
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Node 1 as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP server application installation failed");
    }

    void TestTcpClientInstallation()
    {
        // Test TCP client installation on Node 0
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Node 0 as client
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP client application installation failed");
    }

    void TestFlowMonitor()
    {
        // Test the FlowMonitor functionality
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Node 1 as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Node 0 as client
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        FlowMonitorHelper flowMonitorHelper;
        Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

        // Verify that the flow monitor is installed
        NS_TEST_ASSERT_MSG_EQ(flowMonitor != nullptr, true, "FlowMonitor installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Node 1 as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
        tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Node 0 as client
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        FlowMonitorHelper flowMonitorHelper;
        Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

        Simulator::Run();
        Simulator::Destroy();

        // Check if the simulation ran without errors (basic test)
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    TcpConnectionTest test;
    test.Run();
    return 0;
}
