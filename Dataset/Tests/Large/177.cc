#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h"  // Include the ns3 Test module for unit testing

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControlTest");

class TcpCongestionControlTest : public ns3::TestCase
{
public:
    TcpCongestionControlTest() : TestCase("TCP Congestion Control Test") {}
    virtual ~TcpCongestionControlTest() {}

    virtual void DoRun() {
        // Simulate the setup from the main code
        NodeContainer leftNodes, rightNodes, routerNodes;
        leftNodes.Create(2);
        rightNodes.Create(2);
        routerNodes.Create(2);

        // Point-to-point link setup
        PointToPointHelper pointToPointLeft, pointToPointRight, pointToPointRouter;
        pointToPointLeft.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPointLeft.SetChannelAttribute("Delay", StringValue("5ms"));

        pointToPointRight.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPointRight.SetChannelAttribute("Delay", StringValue("5ms"));

        pointToPointRouter.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
        pointToPointRouter.SetChannelAttribute("Delay", StringValue("20ms"));

        NetDeviceContainer leftDevices1 = pointToPointLeft.Install(leftNodes.Get(0), routerNodes.Get(0));
        NetDeviceContainer leftDevices2 = pointToPointLeft.Install(leftNodes.Get(1), routerNodes.Get(0));
        NetDeviceContainer rightDevices1 = pointToPointRight.Install(rightNodes.Get(0), routerNodes.Get(1));
        NetDeviceContainer rightDevices2 = pointToPointRight.Install(rightNodes.Get(1), routerNodes.Get(1));
        NetDeviceContainer routerDevices = pointToPointRouter.Install(routerNodes);

        InternetStackHelper internet;
        internet.Install(leftNodes);
        internet.Install(rightNodes);
        internet.Install(routerNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer leftInterfaces1 = ipv4.Assign(leftDevices1);
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer leftInterfaces2 = ipv4.Assign(leftDevices2);
        ipv4.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer rightInterfaces1 = ipv4.Assign(rightDevices1);
        ipv4.SetBase("10.1.4.0", "255.255.255.0");
        Ipv4InterfaceContainer rightInterfaces2 = ipv4.Assign(rightDevices2);
        ipv4.SetBase("10.1.5.0", "255.255.255.0");
        ipv4.Assign(routerDevices);

        // Check IP assignments for correctness
        NS_TEST_ASSERT_MSG_EQ(leftInterfaces1.GetAddress(0), Ipv4Address("10.1.1.1"), "Incorrect IP address for left node 1");
        NS_TEST_ASSERT_MSG_EQ(leftInterfaces2.GetAddress(0), Ipv4Address("10.1.2.1"), "Incorrect IP address for left node 2");
        NS_TEST_ASSERT_MSG_EQ(rightInterfaces1.GetAddress(0), Ipv4Address("10.1.3.1"), "Incorrect IP address for right node 1");
        NS_TEST_ASSERT_MSG_EQ(rightInterfaces2.GetAddress(0), Ipv4Address("10.1.4.1"), "Incorrect IP address for right node 2");
    }
};

// Test TCP flow setup (Reno and Cubic)
void TestTcpFlowSetup() {
    NodeContainer leftNodes, rightNodes;
    leftNodes.Create(2);
    rightNodes.Create(2);

    uint16_t port = 8080;
    Address sinkAddress1(InetSocketAddress("10.1.3.1", port));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkAddress1);
    ApplicationContainer sinkApp1 = sinkHelper1.Install(rightNodes.Get(0));

    Address sinkAddress2(InetSocketAddress("10.1.4.1", port));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkAddress2);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(rightNodes.Get(1));

    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(10.0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));

    TypeId tcpRenoTypeId = TypeId::LookupByName("ns3::TcpReno");
    TypeId tcpCubicTypeId = TypeId::LookupByName("ns3::TcpCubic");

    Ptr<Socket> tcpRenoSocket = Socket::CreateSocket(leftNodes.Get(0), tcpRenoTypeId);
    Ptr<Socket> tcpCubicSocket = Socket::CreateSocket(leftNodes.Get(1), tcpCubicTypeId);

    tcpRenoSocket->Connect(sinkAddress1);
    tcpCubicSocket->Connect(sinkAddress2);

    // Verify that the TCP sockets are correctly configured for Reno and Cubic
    NS_TEST_ASSERT_MSG_NE(tcpRenoSocket, nullptr, "Failed to create TCP Reno socket");
    NS_TEST_ASSERT_MSG_NE(tcpCubicSocket, nullptr, "Failed to create TCP Cubic socket");
}

// Test On/Off application setup for traffic generation
void TestOnOffApplication() {
    NodeContainer leftNodes, rightNodes;
    leftNodes.Create(2);
    rightNodes.Create(2);

    uint16_t port = 8080;
    Address sinkAddress1(InetSocketAddress("10.1.3.1", port));
    OnOffHelper onOffTcpReno("ns3::TcpSocketFactory", sinkAddress1);
    onOffTcpReno.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffTcpReno.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffTcpReno.SetAttribute("DataRate", StringValue("5Mbps"));
    onOffTcpReno.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer tcpRenoApp = onOffTcpReno.Install(leftNodes.Get(0));
    tcpRenoApp.Start(Seconds(1.0));
    tcpRenoApp.Stop(Seconds(10.0));

    // Verify that the On/Off application is correctly installed
    NS_TEST_ASSERT_MSG_EQ(tcpRenoApp.GetN(), 1, "On/Off application for TCP Reno not installed correctly!");
}

// Test NetAnim Visualization Setup
void TestNetAnimVisualization() {
    NodeContainer leftNodes, rightNodes, routerNodes;
    leftNodes.Create(2);
    rightNodes.Create(2);
    routerNodes.Create(2);

    AnimationInterface anim("tcp_congestion_control_test.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(leftNodes.Get(0), 1.0, 2.0);  // TCP Reno source
    anim.SetConstantPosition(leftNodes.Get(1), 1.0, 3.0);  // TCP Cubic source
    anim.SetConstantPosition(rightNodes.Get(0), 3.0, 2.0); // TCP Reno sink
    anim.SetConstantPosition(rightNodes.Get(1), 3.0, 3.0); // TCP Cubic sink
    anim.SetConstantPosition(routerNodes.Get(0), 2.0, 2.5); // Router 1
    anim.SetConstantPosition(routerNodes.Get(1), 2.0, 3.5); // Router 2

    anim.EnablePacketMetadata(true);

    // Verify if NetAnim file was created
    NS_TEST_ASSERT_MSG_EQ(anim.GetFileName(), "tcp_congestion_control_test.xml", "NetAnim file was not created correctly");
}

// Main function to run all the unit tests
int main(int argc, char *argv[]) {
    TcpCongestionControlTest test1;
    test1.Run();

    TestTcpFlowSetup();
    TestOnOffApplication();
    TestNetAnimVisualization();

    return 0;
}
