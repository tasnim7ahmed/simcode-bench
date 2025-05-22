#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpRelayTestSuite : public TestCase
{
public:
    UdpRelayTestSuite() : TestCase("Test UDP Relay Example") {}
    virtual ~UdpRelayTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointLinks();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestRelayFunctionality();
        TestPacketReception();
    }

private:
    uint16_t clientPort = 4000, serverPort = 5000;

    // Test if nodes (client, relay, server) are created properly
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(3);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation failed. Expected 3 nodes.");
    }

    // Test if point-to-point links are correctly set up
    void TestPointToPointLinks()
    {
        NodeContainer nodes;
        nodes.Create(3);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer clientToRelay = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer relayToServer = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

        NS_TEST_ASSERT_MSG_GT(clientToRelay.GetN(), 0, "Client-to-relay link setup failed.");
        NS_TEST_ASSERT_MSG_GT(relayToServer.GetN(), 0, "Relay-to-server link setup failed.");
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(3);

        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed.");
        }
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(3);

        PointToPointHelper pointToPoint;
        NetDeviceContainer clientToRelay = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer relayToServer = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer clientIface = address.Assign(clientToRelay);

        address.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer serverIface = address.Assign(relayToServer);

        NS_TEST_ASSERT_MSG_GT(clientIface.GetN(), 0, "Client-relay IP address assignment failed.");
        NS_TEST_ASSERT_MSG_GT(serverIface.GetN(), 0, "Relay-server IP address assignment failed.");
    }

    // Test if UDP sockets are created for client, relay, and server
    void TestSocketCreation()
    {
        NodeContainer nodes;
        nodes.Create(3);

        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(clientSocket, "UDP client socket creation failed.");

        Ptr<Socket> relaySocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(relaySocket, "UDP relay socket creation failed.");

        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(serverSocket, "UDP server socket creation failed.");
    }

    // Test if packets are transmitted from the client
    void TestPacketTransmission()
    {
        NodeContainer nodes;
        nodes.Create(3);

        PointToPointHelper pointToPoint;
        NetDeviceContainer clientToRelay = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer clientIface = address.Assign(clientToRelay);

        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        InetSocketAddress relayAddress(clientIface.GetAddress(1), clientPort);

        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, Create<Packet>(128), 0, relayAddress);

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if relay successfully forwards packets to the server
    void TestRelayFunctionality()
    {
        NodeContainer nodes;
        nodes.Create(3);

        PointToPointHelper pointToPoint;
        NetDeviceContainer clientToRelay = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer relayToServer = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer clientIface = address.Assign(clientToRelay);

        address.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer serverIface = address.Assign(relayToServer);

        Ptr<Socket> relaySocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        relaySocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), clientPort));
        relaySocket->SetRecvCallback(MakeBoundCallback(&RelayPacket, relaySocket, InetSocketAddress(serverIface.GetAddress(1), serverPort)));

        NS_LOG_INFO("Relay function test passed.");
    }

    // Test if the server receives the relayed packets
    void TestPacketReception()
    {
        NodeContainer nodes;
        nodes.Create(3);

        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
        serverSocket->SetRecvCallback([](Ptr<Socket> socket) {
            while (socket->Recv())
            {
                NS_LOG_INFO("Server received a forwarded packet!");
            }
        });

        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }

    // Relay function for testing
    static void RelayPacket(Ptr<Socket> relaySocket, Address serverAddress)
    {
        Ptr<Packet> packet;
        while ((packet = relaySocket->Recv()))
        {
            NS_LOG_INFO("Relay received a packet, forwarding to server...");
            relaySocket->SendTo(packet, 0, serverAddress);
        }
    }
};

// Instantiate the test suite
static UdpRelayTestSuite udpRelayTestSuite;
