#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TcpRttExample
class TcpRttTest : public TestCase
{
public:
    TcpRttTest() : TestCase("Test for TcpRttExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestPointToPointSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestTcpServerInstallation();
        TestTcpClientInstallation();
        TestTcpRttTracing();
        TestSimulationRun();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer ipInterfaces;
    uint16_t port = 8080;

    void TestNodeCreation()
    {
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    void TestPointToPointSetup()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

        devices = pointToPoint.Install(nodes);
        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 1, "Point-to-Point devices not installed correctly");
    }

    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);
        Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4_1 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NE(ipv4_0, nullptr, "Internet stack not installed on Node 0");
        NS_TEST_ASSERT_MSG_NE(ipv4_1, nullptr, "Internet stack not installed on Node 1");
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipInterfaces = ipv4.Assign(devices);
        NS_TEST_ASSERT_MSG_NE(ipInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP for Node 0");
        NS_TEST_ASSERT_MSG_NE(ipInterfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Invalid IP for Node 1");
    }

    void TestTcpServerInstallation()
    {
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on Node 1");
    }

    void TestTcpClientInstallation()
    {
        TcpClientHelper tcpClient(ipInterfaces.GetAddress(1), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on Node 0");
    }

    void TestTcpRttTracing()
    {
        bool tracingConnected = Config::Connect(
            "/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Enqueue",
            MakeNullCallback<void, Ptr<const TcpSocketState>>());
        NS_TEST_ASSERT_MSG_EQ(tracingConnected, true, "Failed to set up TCP RTT tracing");
    }

    void TestSimulationRun()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static TcpRttTest tcpRttTestInstance;
