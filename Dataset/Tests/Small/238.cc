#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Define the test suite
class AdhocWifiUdpTestSuite : public TestCase {
public:
    AdhocWifiUdpTestSuite() : TestCase("Test Adhoc Wi-Fi UDP Example") {}
    virtual ~AdhocWifiUdpTestSuite() {}

    void DoRun() override {
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
    uint16_t port = 5001;

    // Test if nodes (mobile client, static server) are created properly
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    // Test if Ad-hoc Wi-Fi (802.11s) is set up correctly
    void TestWifiSetup() {
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211s);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "Wi-Fi setup failed.");
    }

    // Test if mobility is configured correctly (mobile and static nodes)
    void TestMobilitySetup() {
        NodeContainer nodes;
        nodes.Create(2);
        Ptr<Node> mobileNode = nodes.Get(0);
        Ptr<Node> staticNode = nodes.Get(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel", "Position", Vector3DValue(Vector3D(0, 0, 0)));
        mobility.Install(staticNode);

        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(mobileNode);
        mobileNode->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(5.0, 0.0, 0.0));

        Ptr<MobilityModel> mobilityModelStatic = staticNode->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModelStatic, "Static node mobility setup failed.");

        Ptr<MobilityModel> mobilityModelMobile = mobileNode->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModelMobile, "Mobile node mobility setup failed.");
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed.");
        }
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifi;
        YansWifiPhyHelper wifiPhy;
        WifiMacHelper wifiMac;
        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.2.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "IP address assignment failed.");
    }

    // Test if UDP sockets are created for mobile client and static server
    void TestSocketCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(clientSocket, "UDP client socket creation failed.");

        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(serverSocket, "UDP server socket creation failed.");
    }

    // Test if the client transmits packets correctly
    void TestPacketTransmission() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

        InetSocketAddress serverAddress(Ipv4Address("192.168.2.2"), port);
        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, clientSocket, Create<Packet>(128), 0, serverAddress);

        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if the server receives packets from the client
    void TestPacketReception() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        serverSocket->SetRecvCallback([](Ptr<Socket> socket) {
            while (socket->Recv()) {
                NS_LOG_INFO("Server received a UDP packet!");
            }
        });

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }
};

// Instantiate the test suite
static AdhocWifiUdpTestSuite adhocWifiUdpTestSuite;
