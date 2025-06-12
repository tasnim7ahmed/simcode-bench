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
        TestWifiConfiguration();
        TestMobilityModel();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestPacketTransmission();
    }

private:
    // Test the creation of nodes (1 AP, 1 Client, 1 Server)
    void TestNodeCreation()
    {
        NodeContainer wifiApNode, wifiClientNode, wifiServerNode;
        wifiApNode.Create(1);
        wifiClientNode.Create(1);
        wifiServerNode.Create(1);

        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Access Point node creation failed. Expected 1 node.");
        NS_TEST_ASSERT_MSG_EQ(wifiClientNode.GetN(), 1, "Client node creation failed. Expected 1 node.");
        NS_TEST_ASSERT_MSG_EQ(wifiServerNode.GetN(), 1, "Server node creation failed. Expected 1 node.");
    }

    // Test Wi-Fi network configuration (AP, client, and server)
    void TestWifiConfiguration()
    {
        NodeContainer wifiApNode, wifiClientNode, wifiServerNode;
        wifiApNode.Create(1);
        wifiClientNode.Create(1);
        wifiServerNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        // Configure AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Configure Client and Server
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer clientDevice = wifi.Install(phy, mac, wifiClientNode);
        NetDeviceContainer serverDevice = wifi.Install(phy, mac, wifiServerNode);

        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "AP device configuration failed. Expected 1 device.");
        NS_TEST_ASSERT_MSG_EQ(clientDevice.GetN(), 1, "Client device configuration failed. Expected 1 device.");
        NS_TEST_ASSERT_MSG_EQ(serverDevice.GetN(), 1, "Server device configuration failed. Expected 1 device.");
    }

    // Test mobility model (static positions)
    void TestMobilityModel()
    {
        NodeContainer wifiApNode, wifiClientNode, wifiServerNode;
        wifiApNode.Create(1);
        wifiClientNode.Create(1);
        wifiServerNode.Create(1);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);
        mobility.Install(wifiClientNode);
        mobility.Install(wifiServerNode);

        Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> clientMobility = wifiClientNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> serverMobility = wifiServerNode.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(apMobility, "AP mobility model installation failed.");
        NS_TEST_ASSERT_MSG_NOT_NULL(clientMobility, "Client mobility model installation failed.");
        NS_TEST_ASSERT_MSG_NOT_NULL(serverMobility, "Server mobility model installation failed.");
    }

    // Test the UDP server setup (server socket creation and binding)
    void TestUdpServerSetup()
    {
        NodeContainer wifiServerNode;
        wifiServerNode.Create(1);

        uint16_t port = 8080; // UDP port

        // Setup UDP Server
        Ptr<Socket> serverSocket = Socket::CreateSocket(wifiServerNode.Get(0), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Verifying that the server socket is created and bound correctly
        NS_TEST_ASSERT_MSG_EQ(serverSocket->GetNode()->GetId(), wifiServerNode.Get(0)->GetId(), "UDP server socket binding failed.");
    }

    // Test the UDP client setup (client socket creation)
    void TestUdpClientSetup()
    {
        NodeContainer wifiClientNode;
        wifiClientNode.Create(1);

        uint16_t port = 8080; // UDP port

        Ptr<Socket> clientSocket = Socket::CreateSocket(wifiClientNode.Get(0), UdpSocketFactory::GetTypeId());

        // Verifying that the client socket is created correctly
        NS_TEST_ASSERT_MSG_EQ(clientSocket->GetNode()->GetId(), wifiClientNode.Get(0)->GetId(), "UDP client socket creation failed.");
    }

    // Test packet transmission (client sends packets to the server)
    void TestPacketTransmission()
    {
        NodeContainer wifiApNode, wifiClientNode, wifiServerNode;
        wifiApNode.Create(1);
        wifiClientNode.Create(1);
        wifiServerNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        // Configure AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        // Configure Client and Server
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer clientDevice = wifi.Install(phy, mac, wifiClientNode);
        NetDeviceContainer serverDevice = wifi.Install(phy, mac, wifiServerNode);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);
        mobility.Install(wifiClientNode);
        mobility.Install(wifiServerNode);

        // Install the Internet stack
        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiClientNode);
        stack.Install(wifiServerNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
        Ipv4InterfaceContainer clientInterface = address.Assign(clientDevice);
        Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);

        uint16_t port = 8080; // UDP port

        // Setup UDP Server
        Ptr<Socket> serverSocket = Socket::CreateSocket(wifiServerNode.Get(0), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Setup UDP Client
        Ptr<Socket> clientSocket = Socket::CreateSocket(wifiClientNode.Get(0), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0), &SendPacket, clientSocket, serverInterface.GetAddress(0), port);

        // Run the simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        // Verifying that the server received the packets (log output)
        NS_LOG_UNCOND("Test completed. Check server logs for received packets.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("UdpWifiExampleTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new UdpWifiExampleTestSuite());
    return TestSuiteInstance->Run();
}
