#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class TcpWifiExampleTestSuite : public TestCase
{
public:
    TcpWifiExampleTestSuite() : TestCase("Test TCP over Wi-Fi Example") {}
    virtual ~TcpWifiExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiSetup();
        TestMobilityInstallation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    int numClients = 3;
    uint16_t port = 5000;

    // Test if nodes (clients + server) are created properly
    void TestNodeCreation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), numClients + 1, "Node creation failed. Expected clients + 1 server.");
    }

    // Test if Wi-Fi is correctly set up
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
        Ssid ssid = Ssid("tcp-wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer clientDevices = wifi.Install(phy, mac, wifiNodes.Get(0, numClients));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer serverDevice = wifi.Install(phy, wifiNodes.Get(numClients));

        NS_TEST_ASSERT_MSG_GT(clientDevices.GetN(), 0, "Wi-Fi client setup failed.");
        NS_TEST_ASSERT_MSG_EQ(serverDevice.GetN(), 1, "Wi-Fi server setup failed.");
    }

    // Test if the mobility model is correctly installed
    void TestMobilityInstallation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes);

        for (uint32_t i = 0; i < wifiNodes.GetN(); i++)
        {
            Ptr<MobilityModel> mobilityModel = wifiNodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model installation failed.");
        }
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
        Ssid ssid = Ssid("tcp-wifi-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer clientDevices = wifi.Install(phy, mac, wifiNodes.Get(0, numClients));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer serverDevice = wifi.Install(phy, wifiNodes.Get(numClients));

        InternetStackHelper internet;
        internet.Install(wifiNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);
        Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);

        NS_TEST_ASSERT_MSG_GT(clientInterfaces.GetN(), 0, "Client IP address assignment failed.");
        NS_TEST_ASSERT_MSG_EQ(serverInterface.GetN(), 1, "Server IP address assignment failed.");
    }

    // Test if TCP sockets are created for clients and server
    void TestSocketCreation()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = sink.Install(wifiNodes.Get(numClients));

        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "TCP Server socket creation failed.");

        for (int i = 0; i < numClients; i++)
        {
            OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("192.168.1.1"), port));
            client.SetConstantRate(DataRate("1Mbps"), 512);
            ApplicationContainer clientApp = client.Install(wifiNodes.Get(i));

            NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "TCP Client socket creation failed.");
        }
    }

    // Test if packets are transmitted from the clients
    void TestPacketTransmission()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = sink.Install(wifiNodes.Get(numClients));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(5.0));

        for (int i = 0; i < numClients; i++)
        {
            OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("192.168.1.1"), port));
            client.SetConstantRate(DataRate("1Mbps"), 512);
            ApplicationContainer clientApp = client.Install(wifiNodes.Get(i));
            clientApp.Start(Seconds(1.0 + i));
            clientApp.Stop(Seconds(5.0));
        }

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if the server successfully receives packets
    void TestPacketReception()
    {
        NodeContainer wifiNodes;
        wifiNodes.Create(numClients + 1);

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = sink.Install(wifiNodes.Get(numClients));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(5.0));

        for (int i = 0; i < numClients; i++)
        {
            OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address("192.168.1.1"), port));
            client.SetConstantRate(DataRate("1Mbps"), 512);
            ApplicationContainer clientApp = client.Install(wifiNodes.Get(i));
            clientApp.Start(Seconds(1.0 + i));
            clientApp.Stop(Seconds(5.0));
        }

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(serverApp.Get(0));
        NS_TEST_ASSERT_MSG_GT(sink1->GetTotalRx(), 0, "Packet reception failed.");
    }
};

// Instantiate the test suite
static TcpWifiExampleTestSuite tcpWifiExampleTestSuite;
