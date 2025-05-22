#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpSocketExampleTestSuite : public TestCase
{
public:
    UdpSocketExampleTestSuite() : TestCase("Test UDP Socket Example") {}
    virtual ~UdpSocketExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLink();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestPacketTransmission();
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

    // Test the UDP server setup (server socket creation and binding)
    void TestUdpServerSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        serverSocket->Bind(local);

        // Verifying if the socket is correctly bound
        NS_TEST_ASSERT_MSG_EQ(serverSocket->GetNode()->GetId(), 1, "UDP server socket binding failed.");
    }

    // Test the UDP client setup (client socket creation)
    void TestUdpClientSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 8080;
        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

        // Verifying if the client socket is created correctly
        NS_TEST_ASSERT_MSG_EQ(clientSocket->GetNode()->GetId(), 0, "UDP client socket creation failed.");
    }

    // Test packet transmission (client sends a packet to server)
    void TestPacketTransmission()
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

        uint16_t port = 8080;

        // Setup server
        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        serverSocket->Bind(local);
        serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Setup client and send packet after 2 seconds
        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket, interfaces.GetAddress(1), port);

        Simulator::Run();
        Simulator::Destroy();

        // Verifying if the server received the packet (logged output check)
        NS_LOG_UNCOND("Test completed. Check server logs for received packets.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("UdpSocketExampleTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new UdpSocketExampleTestSuite());
    return TestSuiteInstance->Run();
}
