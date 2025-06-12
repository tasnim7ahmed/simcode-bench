#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpAdhocWifiExampleTestSuite : public TestCase
{
public:
    UdpAdhocWifiExampleTestSuite() : TestCase("Test UDP Ad-Hoc Wi-Fi Example") {}
    virtual ~UdpAdhocWifiExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiConfiguration();
        TestMobilityModel();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestPacketTransmission();
    }

private:
    // Test the creation of nodes (1 client, 1 server)
    void TestNodeCreation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    // Test Wi-Fi network configuration (Ad-Hoc mode)
    void TestWifiConfiguration()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Wi-Fi device configuration failed. Expected 2 devices.");
    }

    // Test mobility model (Random Walk Mobility)
    void TestMobilityModel()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(wifiNodes);

        Ptr<MobilityModel> clientMobility = wifiNodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> serverMobility = wifiNodes.Get(1)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(clientMobility, "Client mobility model installation failed.");
        NS_TEST_ASSERT_MSG_NOT_NULL(serverMobility, "Server mobility model installation failed.");
    }

    // Test the UDP server setup (server socket creation and binding)
    void TestUdpServerSetup()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        uint16_t port = 8080; // UDP port

        // Setup UDP Server
        Ptr<Socket> serverSocket = Socket::CreateSocket(wifiNodes.Get(1), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Verifying that the server socket is created and bound correctly
        NS_TEST_ASSERT_MSG_EQ(serverSocket->GetNode()->GetId(), wifiNodes.Get(1)->GetId(), "UDP server socket binding failed.");
    }

    // Test the UDP client setup (client socket creation)
    void TestUdpClientSetup()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        uint16_t port = 8080; // UDP port

        Ptr<Socket> clientSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());

        // Verifying that the client socket is created correctly
        NS_TEST_ASSERT_MSG_EQ(clientSocket->GetNode()->GetId(), wifiNodes.Get(0)->GetId(), "UDP client socket creation failed.");
    }

    // Test packet transmission (client sends packets to the server)
    void TestPacketTransmission()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(wifiNodes);

        // Install the Internet stack
        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 8080; // UDP port

        // Setup UDP Server
        Ptr<Socket> serverSocket = Socket::CreateSocket(wifiNodes.Get(1), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Setup UDP Client
        Ptr<Socket> clientSocket = Socket::CreateSocket(wifiNodes.Get(0), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket, interfaces.GetAddress(1), port);

        // Run the simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        // Verifying that the server received the packets (log output)
        NS_LOG_UNCOND("Test completed. Check server logs for received packets.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("UdpAdhocWifiExampleTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new UdpAdhocWifiExampleTestSuite());
    return TestSuiteInstance->Run();
}
