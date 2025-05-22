#include "ns3/test.h"
#include "ns3/nstime.h"
#include "ns3/wifi-net-device.h"
#include "ns3/qos-txop.h"
#include "ns3/ssid.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

// Test case to verify node creation
class NodeCreationTest : public TestCase
{
public:
    NodeCreationTest() : TestCase("Verify Node Creation") {}
    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(4);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Node count should be 4");
    }
};

// Test case to verify Wi-Fi installation
class WifiInstallationTest : public TestCase
{
public:
    WifiInstallationTest() : TestCase("Verify Wi-Fi Installation") {}
    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper mac;
        Ssid ssid = Ssid("test-network");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevice = wifi.Install(phy, mac, nodes.Get(0));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(1));

        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), 1, "Station device should be installed");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "AP device should be installed");
    }
};

// Test case to verify mobility setup
class MobilitySetupTest : public TestCase
{
public:
    MobilitySetupTest() : TestCase("Verify Mobility Setup") {}
    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(2);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(10.0, 0.0, 0.0));

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        Ptr<MobilityModel> mob1 = nodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> mob2 = nodes.Get(1)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_EQ(mob1->GetPosition().x, 0.0, "Node 1 position incorrect");
        NS_TEST_ASSERT_MSG_EQ(mob2->GetPosition().x, 10.0, "Node 2 position incorrect");
    }
};

// Test case to verify MAC configuration for TXOP
class MacTxopConfigTest : public TestCase
{
public:
    MacTxopConfigTest() : TestCase("Verify MAC TXOP Configuration") {}
    virtual void DoRun()
    {
        NodeContainer nodes;
        nodes.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper mac;
        Ssid ssid = Ssid("test-network");

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer devices = wifi.Install(phy, mac, nodes.Get(0));

        Ptr<NetDevice> dev = nodes.Get(0)->GetDevice(0);
        Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice>(dev);
        PointerValue ptr;
        Ptr<QosTxop> edca;

        wifi_dev->GetMac()->GetAttribute("BE_Txop", ptr);
        edca = ptr.Get<QosTxop>();

        NS_TEST_ASSERT_MSG_NE(edca, nullptr, "TXOP should be configured for BE traffic");
    }
};

// Test suite for Wi-Fi simulation
class WifiSimulationTestSuite : public TestSuite
{
public:
    WifiSimulationTestSuite() : TestSuite("wifi-simulation-tests", UNIT)
    {
        AddTestCase(new NodeCreationTest, TestCase::QUICK);
        AddTestCase(new WifiInstallationTest, TestCase::QUICK);
        AddTestCase(new MobilitySetupTest, TestCase::QUICK);
        AddTestCase(new MacTxopConfigTest, TestCase::QUICK);
    }
};

// Register the test suite
static WifiSimulationTestSuite wifiSimulationTestSuite;
