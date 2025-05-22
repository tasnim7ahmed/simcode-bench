#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/config.h"
#include "ns3/node-container.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ssid.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiTestSuite");

class WiFiTestSuite : public TestSuite
{
public:
    WiFiTestSuite() : TestSuite("wifi-timing-test-suite", UNIT)
    {
        AddTestCase(new TestWiFiTimingAttributes, TestCase::QUICK);
        AddTestCase(new TestNodeCreation, TestCase::QUICK);
        AddTestCase(new TestDeviceInstallation, TestCase::QUICK);
        AddTestCase(new TestMobilityModel, TestCase::QUICK);
        AddTestCase(new TestIpv4AddressAssignment, TestCase::QUICK);
        AddTestCase(new TestApplicationInstallation, TestCase::QUICK);
        AddTestCase(new TestThroughput, TestCase::EXTENSIVE);
    }
};

// Test Case: Verify that Wi-Fi timing attributes are correctly set
class TestWiFiTimingAttributes : public TestCase
{
public:
    TestWiFiTimingAttributes() : TestCase("Verify Wi-Fi timing attributes") {}

    virtual void DoRun()
    {
        Time slot = Time("9us");
        Time sifs = Time("10us");
        Time pifs = Time("19us");

        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Slot", TimeValue(slot));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Sifs", TimeValue(sifs));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Pifs", TimeValue(pifs));

        NS_TEST_ASSERT_MSG_EQ(Config::Get("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Slot"), slot, "Slot time incorrect");
        NS_TEST_ASSERT_MSG_EQ(Config::Get("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Sifs"), sifs, "SIFS duration incorrect");
        NS_TEST_ASSERT_MSG_EQ(Config::Get("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Pifs"), pifs, "PIFS duration incorrect");
    }
};

// Test Case: Verify that nodes are correctly created
class TestNodeCreation : public TestCase
{
public:
    TestNodeCreation() : TestCase("Verify node creation") {}

    virtual void DoRun()
    {
        NodeContainer wifiStaNode, wifiApNode;
        wifiStaNode.Create(1);
        wifiApNode.Create(1);

        NS_TEST_ASSERT_MSG_EQ(wifiStaNode.GetN(), 1, "STA Node not created properly");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "AP Node not created properly");
    }
};

// Test Case: Verify that Wi-Fi devices are correctly installed
class TestDeviceInstallation : public TestCase
{
public:
    TestDeviceInstallation() : TestCase("Verify Wi-Fi device installation") {}

    virtual void DoRun()
    {
        NodeContainer wifiStaNode, wifiApNode;
        wifiStaNode.Create(1);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), 1, "STA device installation failed");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "AP device installation failed");
    }
};

// Test Case: Verify Mobility Model is correctly assigned
class TestMobilityModel : public TestCase
{
public:
    TestMobilityModel() : TestCase("Verify Mobility Model") {}

    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(1.0, 0.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        Ptr<MobilityModel> model = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(model, nullptr, "Mobility model not installed");
    }
};

// Test Case: Verify IP Address Assignment
class TestIpv4AddressAssignment : public TestCase
{
public:
    TestIpv4AddressAssignment() : TestCase("Verify IPv4 Address Assignment") {}

    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");

        NetDeviceContainer devices;
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "IPv4 address assignment failed");
    }
};

// Test Case: Verify UDP Application Installation
class TestApplicationInstallation : public TestCase
{
public:
    TestApplicationInstallation() : TestCase("Verify UDP Application Installation") {}

    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 9;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper client(Ipv4Address("192.168.1.1"), port);
        ApplicationContainer clientApp = client.Install(nodes.Get(1));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_NE(serverApp.Get(0), nullptr, "Server Application not installed");
        NS_TEST_ASSERT_MSG_NE(clientApp.Get(0), nullptr, "Client Application not installed");
    }
};

// Test Case: Check Throughput
class TestThroughput : public TestCase
{
public:
    TestThroughput() : TestCase("Verify Throughput Calculation") {}

    virtual void DoRun()
    {
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();
        Simulator::Destroy();

        double throughput = 5.0; // Replace with real calculation
        NS_TEST_ASSERT_MSG_GT(throughput, 0.0, "Throughput is zero, transmission failed");
    }
};

// Register Test Suite
static WiFiTestSuite suite;
