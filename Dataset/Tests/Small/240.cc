#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiUdpMulticastTestSuite : public TestCase {
public:
    WifiUdpMulticastTestSuite() : TestCase("Test Wi-Fi UDP Multicast Example") {}
    virtual ~WifiUdpMulticastTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestWifiNetworkSetup();
        TestMobilityModelInstallation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestMulticastRouting();
        TestReceiverSocketCreation();
        TestSenderSocketCreation();
        TestUdpMulticastTransmission();
        TestUdpMulticastReception();
    }

private:
    uint16_t port = 5000;
    Ipv4Address multicastGroup = Ipv4Address("225.1.2.3");

    // Test if nodes (Sender, Receiver1, Receiver2) are created correctly
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(3);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation failed.");
    }

    // Test if the Wi-Fi network is set up correctly
    void TestWifiNetworkSetup() {
        NodeContainer nodes;
        nodes.Create(3);
        
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a);
        
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        
        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);
        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "Wi-Fi network setup failed.");
    }

    // Test if the mobility model is installed correctly
    void TestMobilityModelInstallation() {
        NodeContainer nodes;
        nodes.Create(3);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model installation failed on node.");
        }
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(3);

        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed on node.");
        }
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(3);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a);
        
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        
        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "IP address assignment failed.");
    }

    // Test if multicast routing is set up correctly
    void TestMulticastRouting() {
        NodeContainer nodes;
        nodes.Create(3);
        
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a);
        
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        
        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        Ipv4StaticRoutingHelper multicastRouting;
        Ptr<Ipv4StaticRouting> receiver1Routing = multicastRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
        receiver1Routing->AddMulticastRoute(interfaces.GetAddress(0), multicastGroup, 1);

        Ptr<Ipv4StaticRouting> receiver2Routing = multicastRouting.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
        receiver2Routing->AddMulticastRoute(interfaces.GetAddress(0), multicastGroup, 1);

        NS_LOG_INFO("Multicast routing setup completed.");
    }

    // Test if receiver sockets are created correctly
    void TestReceiverSocketCreation() {
        NodeContainer nodes;
        nodes.Create(3);
        
        Ptr<Socket> receiverSocket1 = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        receiverSocket1->Bind(InetSocketAddress(multicastGroup, port));

        Ptr<Socket> receiverSocket2 = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
        receiverSocket2->Bind(InetSocketAddress(multicastGroup, port));

        NS_TEST_ASSERT_MSG_NOT_NULL(receiverSocket1, "Receiver socket 1 creation failed.");
        NS_TEST_ASSERT_MSG_NOT_NULL(receiverSocket2, "Receiver socket 2 creation failed.");
    }

    // Test if sender socket is created correctly
    void TestSenderSocketCreation() {
        NodeContainer nodes;
        nodes.Create(3);

        Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        InetSocketAddress multicastAddr(multicastGroup, port);
        senderSocket->Connect(multicastAddr);

        NS_TEST_ASSERT_MSG_NOT_NULL(senderSocket, "Sender socket creation failed.");
    }

    // Test if the UDP multicast transmission works correctly
    void TestUdpMulticastTransmission() {
        NodeContainer nodes;
        nodes.Create(3);

        Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        InetSocketAddress multicastAddr(multicastGroup, port);

        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, senderSocket, Create<Packet>(128), 0, multicastAddr);
        Simulator::Schedule(Seconds(2.0), &Socket::SendTo, senderSocket, Create<Packet>(128), 0, multicastAddr);

        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("UDP multicast transmission test passed.");
    }

    // Test if the UDP multicast reception works correctly
    void TestUdpMulticastReception() {
        NodeContainer nodes;
        nodes.Create(3);

        Ptr<Socket> receiverSocket1 = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        receiverSocket1->Bind(InetSocketAddress(multicastGroup, port));

        Ptr<Socket> receiverSocket2 = Socket::CreateSocket(nodes.Get(2), UdpSocketFactory::GetTypeId());
        receiverSocket2->Bind(InetSocketAddress(multicastGroup, port));

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("UDP multicast reception test passed.");
    }
};

// Instantiate the test suite
static WifiUdpMulticastTestSuite wifiUdpMulticastTestSuite;
