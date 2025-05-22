#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class PointToPointUdpEchoTestSuite : public TestCase
{
public:
    PointToPointUdpEchoTestSuite() : TestCase("Test Point-to-Point UDP Echo Example") {}
    virtual ~PointToPointUdpEchoTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLink();
        TestUdpEchoServer();
        TestUdpEchoClient();
        TestSimulation();
    }

private:
    // Test the creation of two nodes (client and server)
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    // Test the point-to-point link configuration
    void TestPointToPointLink()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-point link failed. Expected 2 devices.");
    }

    // Test the UDP Echo Server setup
    void TestUdpEchoServer()
    {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 9;
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP Echo Server application setup failed.");
    }

    // Test the UDP Echo Client setup
    void TestUdpEchoClient()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 9;
        UdpEchoServerHelper echoServer(port);
        echoServer.Install(nodes.Get(1));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP Echo Client application setup failed.");
    }

    // Test if the simulation runs correctly
    void TestSimulation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 9;
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // After running the simulation, verify that no errors occurred
        NS_TEST_ASSERT_MSG_TRUE(true, "Simulation failed or produced an error.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("PointToPointUdpEchoTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new PointToPointUdpEchoTestSuite());
    return TestSuiteInstance->Run();
}
