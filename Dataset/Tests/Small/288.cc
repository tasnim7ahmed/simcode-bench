#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class WifiTcpTestSuite : public TestSuite
{
public:
    WifiTcpTestSuite() : TestSuite("wifi-tcp-tests", UNIT)
    {
        AddTestCase(new TestNodeCreation(), TestCase::QUICK);
        AddTestCase(new TestWifiInstallation(), TestCase::QUICK);
        AddTestCase(new TestIpAssignment(), TestCase::QUICK);
        AddTestCase(new TestTcpCommunication(), TestCase::EXTENSIVE);
    }
};

// Test if nodes are correctly created
class TestNodeCreation : public TestCase
{
public:
    TestNodeCreation() : TestCase("Test Node Creation") {}

    virtual void DoRun()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 2, "Should have 2 STA nodes");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Should have 1 AP node");
    }
};

// Test if Wi-Fi devices are installed correctly
class TestWifiInstallation : public TestCase
{
public:
    TestWifiInstallation() : TestCase("Test Wi-Fi Installation") {}

    virtual void DoRun()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211ac);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("VhtMcs9"),
                                     "ControlMode", StringValue("VhtMcs0"));

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-80211ac");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        NS_TEST_ASSERT_MSG_GT(staDevices.GetN(), 0, "STA devices should be installed");
        NS_TEST_ASSERT_MSG_GT(apDevice.GetN(), 0, "AP device should be installed");
    }
};

// Test if IP addresses are assigned correctly
class TestIpAssignment : public TestCase
{
public:
    TestIpAssignment() : TestCase("Test IP Assignment") {}

    virtual void DoRun()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        InternetStackHelper internet;
        internet.Install(wifiStaNodes);
        internet.Install(wifiApNode);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("192.168.1.0", "255.255.255.0");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-80211ac");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);

        NS_TEST_ASSERT_MSG_NE(staInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"),
                              "STA should have a valid IP");
        NS_TEST_ASSERT_MSG_NE(apInterface.GetAddress(0), Ipv4Address("0.0.0.0"),
                              "AP should have a valid IP");
    }
};

// Test TCP communication between STA nodes
class TestTcpCommunication : public TestCase
{
public:
    TestTcpCommunication() : TestCase("Test TCP Communication") {}

    virtual void DoRun()
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(2);
        wifiApNode.Create(1);

        InternetStackHelper internet;
        internet.Install(wifiStaNodes);
        internet.Install(wifiApNode);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("192.168.1.0", "255.255.255.0");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-80211ac");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);

        uint16_t port = 5000;
        PacketSinkHelper tcpServer("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApp = tcpServer.Install(wifiStaNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        OnOffHelper tcpClient("ns3::TcpSocketFactory",
                              InetSocketAddress(staInterfaces.GetAddress(0), port));
        tcpClient.SetAttribute("DataRate", StringValue("100Mbps"));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = tcpClient.Install(wifiStaNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();

        Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
        NS_TEST_ASSERT_MSG_GT(sink->GetTotalRx(), 0, "Server should receive some data");

        Simulator::Destroy();
    }
};

static WifiTcpTestSuite wifiTcpTestSuite;
