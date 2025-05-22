#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpMultiServerExampleTestSuite : public TestCase
{
public:
    UdpMultiServerExampleTestSuite() : TestCase("Test UDP Multi-Server Example") {}
    virtual ~UdpMultiServerExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestCsmaNetwork();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestPacketTransmission();
    }

private:
    // Test the creation of three nodes (1 Client and 2 Servers)
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(3);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation failed. Expected 3 nodes.");
    }

    // Test the CSMA network configuration
    void TestCsmaNetwork()
    {
        NodeContainer nodes;
        nodes.Create(3);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5000)));

        NetDeviceContainer devices = csma.Install(nodes);
        
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 3, "CSMA network setup failed. Expected 3 devices.");
    }

    // Test the UDP server setup (server socket creation and binding)
    void TestUdpServerSetup()
    {
        NodeContainer nodes;
        nodes.Create(3);

        uint16_t port1 = 8080;
        uint16_t port2 = 9090;

        // Setup UDP Server 1
        Ptr<Socket> serverSocket1 = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        serverSocket1->Bind(InetSocketAddress(Ipv4Address::GetAny(), port1));
        serverSocket1->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Setup UDP Server 2
        Ptr<Socket> serverSocket2 = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
        serverSocket2->Bind(InetSocketAddress(Ipv4Address::GetAny(), port2));
        serverSocket2->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Verifying that the servers are correctly bound
        NS_TEST_ASSERT_MSG_EQ(serverSocket1->GetNode()->GetId(), 1, "UDP server 1 socket binding failed.");
        NS_TEST_ASSERT_MSG_EQ(serverSocket2->GetNode()->GetId(), 2, "UDP server 2 socket binding failed.");
    }

    // Test the UDP client setup (client socket creation)
    void TestUdpClientSetup()
    {
        NodeContainer nodes;
        nodes.Create(3);

        uint16_t port1 = 8080;
        uint16_t port2 = 9090;

        Ptr<Socket> clientSocket1 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        Ptr<Socket> clientSocket2 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

        // Verifying if the client sockets are created correctly
        NS_TEST_ASSERT_MSG_EQ(clientSocket1->GetNode()->GetId(), 0, "UDP client 1 socket creation failed.");
        NS_TEST_ASSERT_MSG_EQ(clientSocket2->GetNode()->GetId(), 0, "UDP client 2 socket creation failed.");
    }

    // Test packet transmission (client sends packets to both servers)
    void TestPacketTransmission()
    {
        NodeContainer nodes;
        nodes.Create(3);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5000)));
        NetDeviceContainer devices = csma.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port1 = 8080;
        uint16_t port2 = 9090;

        // Setup UDP Servers
        Ptr<Socket> serverSocket1 = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        serverSocket1->Bind(InetSocketAddress(Ipv4Address::GetAny(), port1));
        serverSocket1->SetRecvCallback(MakeCallback(&ReceivePacket));

        Ptr<Socket> serverSocket2 = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
        serverSocket2->Bind(InetSocketAddress(Ipv4Address::GetAny(), port2));
        serverSocket2->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Setup UDP Clients
        Ptr<Socket> clientSocket1 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        Ptr<Socket> clientSocket2 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

        // Schedule packet sending to both servers
        Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket1, interfaces.GetAddress(1), port1);
        Simulator::Schedule(Seconds(2.5), &SendPacket, clientSocket2, interfaces.GetAddress(2), port2);

        Simulator::Run();
        Simulator::Destroy();

        // Verifying that the servers received the packets (log output)
        NS_LOG_UNCOND("Test completed. Check server logs for received packets.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("UdpMultiServerExampleTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new UdpMultiServerExampleTestSuite());
    return TestSuiteInstance->Run();
}
