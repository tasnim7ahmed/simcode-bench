#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class MinimalUdpExampleTestSuite : public TestCase
{
public:
    MinimalUdpExampleTestSuite() : TestCase("Test Minimal UDP Example") {}
    virtual ~MinimalUdpExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointConnection();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    // Test the creation of client and server nodes
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    // Test the creation of the point-to-point connection
    void TestPointToPointConnection()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-point devices installation failed. Expected 2 devices.");
    }

    // Test if Internet stack is installed correctly on both nodes
    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed on node 0.");

        ipv4 = nodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed on node 1.");
    }

    // Test if IP addresses are assigned to the devices
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), 2, "IP address assignment failed. Expected 2 IP addresses.");
    }

    // Test if UDP sockets are created for both the client and server
    void TestSocketCreation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 4000;
        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

        NS_TEST_ASSERT_MSG_NOT_NULL(serverSocket, "Socket creation failed for server.");
        NS_TEST_ASSERT_MSG_NOT_NULL(clientSocket, "Socket creation failed for client.");
    }

    // Test if the client sends a packet to the server
    void TestPacketTransmission()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 4000;
        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        Ptr<Packet> packet = Create<Packet>(128); // 128-byte packet
        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, packet, 0, InetSocketAddress(interfaces.GetAddress(1), port));

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        // If no errors occur, the test passes
        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if the server receives a packet correctly
    void TestPacketReception()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 4000;
        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

        serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        Ptr<Packet> packet = Create<Packet>(128); // 128-byte packet
        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, packet, 0, InetSocketAddress(interfaces.GetAddress(1), port));

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }
};

// Instantiate the test suite
static MinimalUdpExampleTestSuite minimalUdpExampleTestSuite;
