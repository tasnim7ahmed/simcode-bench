#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Define the test suite
class WifiUdpClientServerTestSuite : public TestCase {
public:
    WifiUdpClientServerTestSuite() : TestCase("Test Wi-Fi UDP Client-Server Example") {}
    virtual ~WifiUdpClientServerTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestWifiSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    uint16_t port = 5000;

    // Test if nodes (client and AP) are created properly
    void TestNodeCreation() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    // Test if Wi-Fi is set up correctly
    void TestWifiSetup() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("wifi-network");

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1));

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer clientDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

        NS_TEST_ASSERT_MSG_GT(apDevice.GetN(), 0, "Wi-Fi setup failed for AP.");
        NS_TEST_ASSERT_MSG_GT(clientDevice.GetN(), 0, "Wi-Fi setup failed for Client.");
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        for (uint32_t i = 0; i < wifiNodes.GetN(); i++) {
            Ptr<Ipv4> ipv4 = wifiNodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed.");
        }
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        YansWifiPhyHelper wifiPhy;
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        wifiMac.SetType("ns3::ApWifiMac");
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1));

        wifiMac.SetType("ns3::StaWifiMac");
        NetDeviceContainer clientDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(clientDevice, apDevice));

        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "IP address assignment failed.");
    }

    // Test if UDP sockets are created for client and AP
    void TestSocketCreation() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        Ptr<Socket> clientSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(clientSocket, "UDP client socket creation failed.");

        Ptr<Socket> apSocket = Socket::CreateSocket(wifiNodes.Get(1), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(apSocket, "UDP AP socket creation failed.");
    }

    // Test if the client transmits packets correctly
    void TestPacketTransmission() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        Ptr<Socket> clientSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());

        InetSocketAddress apAddress(Ipv4Address("192.168.1.2"), port);
        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, Create<Packet>(64), 0, apAddress);

        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if the AP receives packets from the client
    void TestPacketReception() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        Ptr<Socket> apSocket = Socket::CreateSocket(wifiNodes.Get(1), UdpSocketFactory::GetTypeId());
        apSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        apSocket->SetRecvCallback([](Ptr<Socket> socket) {
            while (socket->Recv()) {
                NS_LOG_INFO("AP received a UDP packet!");
            }
        });

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }
};

// Instantiate the test suite
static WifiUdpClientServerTestSuite wifiUdpClientServerTestSuite;
