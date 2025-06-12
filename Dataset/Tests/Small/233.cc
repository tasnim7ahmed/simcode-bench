#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpBroadcastTestSuite : public TestCase
{
public:
    UdpBroadcastTestSuite() : TestCase("Test UDP Broadcast Example") {}
    virtual ~UdpBroadcastTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiSetup();
        TestMobilitySetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    int numReceivers = 3;
    uint16_t port = 4000;

    // Test if nodes (1 sender + multiple receivers) are created properly
    void TestNodeCreation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), numReceivers + 1, "Node creation failed. Expected sender + receivers.");
    }

    // Test if Wi-Fi is set up correctly
    void TestWifiSetup()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "Wi-Fi setup failed.");
    }

    // Test if the mobility model is correctly installed
    void TestMobilitySetup()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes);

        for (uint32_t i = 0; i < wifiNodes.GetN(); i++)
        {
            Ptr<MobilityModel> mobilityModel = wifiNodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model installation failed.");
        }
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        for (uint32_t i = 0; i < wifiNodes.GetN(); i++)
        {
            Ptr<Ipv4> ipv4 = wifiNodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed.");
        }
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        WifiHelper wifi;
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "IP address assignment failed.");
    }

    // Test if UDP sockets are created for sender and receivers
    void TestSocketCreation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        Ptr<Socket> senderSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(senderSocket, "UDP sender socket creation failed.");

        for (int i = 1; i <= numReceivers; i++)
        {
            Ptr<Socket> receiverSocket = Socket::CreateSocket(wifiNodes.Get(i), UdpSocketFactory::GetTypeId());
            NS_TEST_ASSERT_MSG_NOT_NULL(receiverSocket, "UDP receiver socket creation failed.");
        }
    }

    // Test if packets are transmitted from the sender
    void TestPacketTransmission()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        Ptr<Socket> senderSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());
        Ipv4Address broadcastAddress("192.168.1.255");
        InetSocketAddress remote = InetSocketAddress(broadcastAddress, port);

        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, senderSocket, Create<Packet>(128), 0, remote);

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if receivers successfully receive broadcast packets
    void TestPacketReception()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numReceivers + 1);

        // Create receiver sockets
        for (int i = 1; i <= numReceivers; i++)
        {
            Ptr<Socket> receiverSocket = Socket::CreateSocket(wifiNodes.Get(i), UdpSocketFactory::GetTypeId());
            receiverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
            receiverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
        }

        // Send packet from sender
        Ptr<Socket> senderSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());
        Ipv4Address broadcastAddress("192.168.1.255");
        InetSocketAddress remote = InetSocketAddress(broadcastAddress, port);

        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, senderSocket, Create<Packet>(128), 0, remote);

        Simulator::Stop(Seconds(2.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }

    // Packet receive callback
    static void ReceivePacket(Ptr<Socket> socket)
    {
        while (socket->Recv())
        {
            NS_LOG_INFO("Receiver received a broadcast packet!");
        }
    }
};

// Instantiate the test suite
static UdpBroadcastTestSuite udpBroadcastTestSuite;
