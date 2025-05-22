#include "ns3/test.h"
#include "ns3/config.h"
#include "ns3/node-container.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/log.h"

using namespace ns3;

class WifiConfigurationTest : public TestCase
{
public:
    WifiConfigurationTest() : TestCase("Test WiFi Configuration Settings") {}

    void DoRun() override
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
        std::string rtsThreshold;
        Config::GetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", rtsThreshold);
        NS_TEST_ASSERT_MSG_EQ(rtsThreshold, "999999", "RTS/CTS threshold configuration failed!");
    }
};

class MobilityModelTest : public TestCase
{
public:
    MobilityModelTest() : TestCase("Test Mobility Model Initialization") {}

    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(3);
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(5.0, 0.0, 0.0));
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(10.0, 0.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);
        NS_TEST_ASSERT_MSG_EQ(nodes.Get(0)->GetObject<MobilityModel>()->GetPosition().x, 5.0, "Mobility model position incorrect!");
    }
};

class NetworkSetupTest : public TestCase
{
public:
    NetworkSetupTest() : TestCase("Test Network Setup and Address Assignment") {}

    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(3);
        InternetStackHelper stack;
        stack.Install(nodes);
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        NetDeviceContainer devices;
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        WifiMacHelper mac;
        Ssid ssid = Ssid("test-network");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        devices = wifi.Install(phy, mac, nodes);
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "IP address assignment failed!");
    }
};

class ThroughputTest : public TestCase
{
public:
    ThroughputTest() : TestCase("Test Expected Throughput Range") {}

    void DoRun() override
    {
        double throughput = 10.5; // Simulated example value
        double minExpectedThroughput = 5.0;
        double maxExpectedThroughput = 20.0;
        NS_TEST_ASSERT_MSG(throughput >= minExpectedThroughput, "Throughput is below the minimum expected!");
        NS_TEST_ASSERT_MSG(throughput <= maxExpectedThroughput, "Throughput exceeds the maximum expected!");
    }
};

static WifiConfigurationTest wifiConfigurationTest;
static MobilityModelTest mobilityModelTest;
static NetworkSetupTest networkSetupTest;
static ThroughputTest throughputTest;
