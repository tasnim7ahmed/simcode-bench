#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Wired TCP Communication
class WiredTcpExampleTest : public TestCase
{
public:
    WiredTcpExampleTest() : TestCase("Test for Wired TCP Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointConnection();
        TestIpAddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    void TestNodeCreation()
    {
        nodes.Create(2); // Create two nodes (Computer 1 and Computer 2)
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    void TestPointToPointConnection()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

        devices = pointToPoint.Install(nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install devices on nodes");
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned to both nodes
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    void TestTcpServerSetup()
    {
        TcpServerHelper tcpServer(9); // Listening on port 9
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on Computer 2");
    }

    void TestTcpClientSetup()
    {
        TcpClientHelper tcpClient(interfaces.GetAddress(1), 9); // Connecting to Computer 2
        tcpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Send 10 packets
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets

        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Installing client on Computer 1
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on Computer 1");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WiredTcpExampleTest wiredTcpExampleTestInstance;
