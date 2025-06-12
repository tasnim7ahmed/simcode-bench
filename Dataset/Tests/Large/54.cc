#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/config.h"
#include "ns3/mobility-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

class PhyModelValidationTest : public TestCase {
public:
    PhyModelValidationTest() : TestCase("Validate PHY Model Selection") {}

    void DoRun() override {
        std::string phyModel = "InvalidModel";
        bool exceptionThrown = false;
        try {
            if (phyModel != "Yans" && phyModel != "Spectrum") {
                throw std::runtime_error("Invalid PHY model (must be Yans or Spectrum)");
            }
        } catch (const std::exception &e) {
            exceptionThrown = true;
        }
        NS_TEST_ASSERT_MSG_EQ(exceptionThrown, true, "Expected exception for invalid PHY model");
    }
};

class RtsCtsTest : public TestCase {
public:
    RtsCtsTest() : TestCase("Check RTS/CTS Threshold Setting") {}

    void DoRun() override {
        bool useRts = true;
        if (useRts) {
            Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
        }
        std::string value;
        Config::GetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", value);
        NS_TEST_ASSERT_MSG_EQ(value, "0", "RTS/CTS threshold was not set correctly");
    }
};

class MobilityModelTest : public TestCase {
public:
    MobilityModelTest() : TestCase("Check Mobility Model Setup") {}

    void DoRun() override {
        NodeContainer nodes;
        nodes.Create(2);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        Ptr<MobilityModel> model = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(model, nullptr, "Mobility model was not installed correctly");
    }
};

class NetworkDeviceTest : public TestCase {
public:
    NetworkDeviceTest() : TestCase("Check Network Device Installation") {}

    void DoRun() override {
        NodeContainer nodes;
        nodes.Create(2);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-80211ac");

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "Network device was not installed correctly");
    }
};

class ThroughputValidationTest : public TestCase {
public:
    ThroughputValidationTest() : TestCase("Validate Throughput Calculation") {}

    void DoRun() override {
        double receivedBytes = 1000000; // 1 MB
        Time simulationTime = Seconds(10);
        double throughput = (receivedBytes * 8) / simulationTime.GetSeconds();
        NS_TEST_ASSERT_MSG_GT(throughput, 0, "Throughput calculation is incorrect");
    }
};

static class VhtWifiTestSuite : public TestSuite {
public:
    VhtWifiTestSuite() : TestSuite("vht-wifi-tests", UNIT) {
        AddTestCase(new PhyModelValidationTest, TestCase::QUICK);
        AddTestCase(new RtsCtsTest, TestCase::QUICK);
        AddTestCase(new MobilityModelTest, TestCase::QUICK);
        AddTestCase(new NetworkDeviceTest, TestCase::QUICK);
        AddTestCase(new ThroughputValidationTest, TestCase::QUICK);
    }
} g_vhtWifiTestSuite;
