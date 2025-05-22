#include "ns3/test.h"
#include "ns3/node.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

class NodeCreationTest : public TestCase {
public:
    NodeCreationTest() : TestCase("Node Creation Test") {}
    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Incorrect number of nodes created");
    }
};

class PhyLayerSetupTest : public TestCase {
public:
    PhyLayerSetupTest() : TestCase("PHY Layer Setup Test") {}
    virtual void DoRun() {
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());
        
        NS_TEST_ASSERT_MSG_NE(phy.GetChannel(), nullptr, "PHY Channel not properly initialized");
    }
};

class MCSDataRateTest : public TestCase {
public:
    MCSDataRateTest() : TestCase("MCS Data Rate Test") {}
    virtual void DoRun() {
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211ax);
        
        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        
        NetDeviceContainer devices;
        NodeContainer nodes;
        nodes.Create(1);
        
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());
        
        devices = wifi.Install(phy, mac, nodes);
        
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 1, "Device not installed correctly");
    }
};

static class Ns3UnitTestSuite : public TestSuite {
public:
    Ns3UnitTestSuite() : TestSuite("ns3-unit-tests", UNIT) {
        AddTestCase(new NodeCreationTest, TestCase::QUICK);
        AddTestCase(new PhyLayerSetupTest, TestCase::QUICK);
        AddTestCase(new MCSDataRateTest, TestCase::QUICK);
    }
} g_ns3UnitTestSuite;
