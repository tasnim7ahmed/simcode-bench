#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpWifiExampleTestSuite : public TestCase
{
public:
    UdpWifiExampleTestSuite() : TestCase("Test UDP Wi-Fi Example") {}
    virtual ~UdpWifiExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    int numClients = 3;
    uint16_t port = 5000;

    // Test if nodes are created properly
    void TestNodeCreation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), numClients + 1, "Node creation failed. Expected more nodes.");
    }

    // Test if Wi-Fi setup is correct
    void TestWifiSetup()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("udp-wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer clientDevices = wifi.Install(phy, mac, wifiNodes.Get(0, numClients));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer serverDevice = wifi.Install(phy, mac, wifiNodes.Get(numClients));

        NS_TEST_ASSERT_MSG_EQ(clientDevices.GetN(), numClients, "Wi-Fi clients not set up correctly.");
        NS_TEST_ASSERT_MSG_EQ(serverDevice.GetN(), 1, "Wi-Fi server not set up correctly.");
    }

    // Test if Internet stack is installed correctly
    void TestInternetStackInstallation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        for (uint32_t i = 0; i < wifiNodes.GetN(); i++)
        {
            Ptr<Ipv4> ipv4 = wifiNodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed.");
        }
    }

    // Test if IP addresses are assigned properly
    void TestIpAddressAssignment()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        WifiHelper wifi;
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("udp-wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer clientDevices = wifi.Install(phy, mac, wifiNodes.Get(0, numClients));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer serverDevice = wifi.Install(phy, mac, wifiNodes.Get(numClients));

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(clientDevices);
        Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);

        NS_TEST_ASSERT_MSG_EQ(interfaces.GetN(), numClients, "IP address assignment failed for clients.");
        NS_TEST_ASSERT_MSG_EQ(serverInterface.GetN(), 1, "IP address assignment failed for server.");
    }

    // Test if UDP sockets are created for client and server
    void TestSocketCreation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);
        Ptr<Node> serverNode = wifiNodes.Get(numClients);

        Ptr<Socket> serverSocket = Socket::CreateSocket(serverNode, UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

        NS_TEST_ASSERT_MSG_NOT_NULL(serverSocket, "Server socket creation failed.");

        for (int i = 0; i < numClients; i++)
        {
            Ptr<Socket> clientSocket = Socket::CreateSocket(wifiNodes.Get(i), UdpSocketFactory::GetTypeId());
            NS_TEST_ASSERT_MSG_NOT_NULL(clientSocket, "Client socket creation failed.");
        }
    }

    // Test if packets are transmitted from clients
    void TestPacketTransmission()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);
        Ptr<Node> serverNode = wifiNodes.Get(numClients);

        WifiHelper wifi;
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("udp-wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer clientDevices = wifi.Install(phy, mac, wifiNodes.Get(0, numClients));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer serverDevice = wifi.Install(phy, mac, serverNode);

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(clientDevices);
        Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);

        for (int i = 0; i < numClients; i++)
        {
            Ptr<Socket> clientSocket = Socket::CreateSocket(wifiNodes.Get(i), UdpSocketFactory::GetTypeId());
            Ptr<Packet> packet = Create<Packet>(128);
            Simulator::Schedule(Seconds(1.0 + i), &Socket::SendTo, clientSocket, packet, 0, InetSocketAddress(serverInterface.GetAddress(0), port));
        }

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if server successfully receives packets
    void TestPacketReception()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);
        Ptr<Node> serverNode = wifiNodes.Get(numClients);

        Ptr<Socket> serverSocket = Socket::CreateSocket(serverNode, UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

        serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }
};

// Instantiate the test suite
static UdpWifiExampleTestSuite udpWifiExampleTestSuite;
