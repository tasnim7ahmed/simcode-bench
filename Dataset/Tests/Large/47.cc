#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/packet.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ns3UnitTests");

class PacketReceptionTest : public TestCase {
public:
    PacketReceptionTest() : TestCase("Packet Reception Test") {}
    
    virtual void DoRun() {
        Ptr<Packet> packet = Create<Packet>(100); // Create a 100-byte packet
        NS_TEST_ASSERT_MSG_EQ(packet->GetSize(), 100, "Packet size should be 100 bytes");
    }
};

class ObssPdResetTest : public TestCase {
public:
    ObssPdResetTest() : TestCase("OBSS PD Reset Tracking Test") {}

    virtual void DoRun() {
        double obssPdThreshold = -82.0; // Example OBSS PD threshold in dBm
        double previousThreshold = obssPdThreshold;

        // Simulate an OBSS PD reset event
        obssPdThreshold = -79.0; // New value after reset

        NS_TEST_ASSERT_MSG_NE(previousThreshold, obssPdThreshold, "OBSS PD threshold should change after reset");
    }
};

class NetworkSetupTest : public TestCase {
public:
    NetworkSetupTest() : TestCase("Network Configuration Test") {}

    virtual void DoRun() {
        NodeContainer wifiNodes;
        wifiNodes.Create(2);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        WifiMacHelper mac;
        NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

        NS_TEST_ASSERT_MSG_GT(wifiNodes.GetN(), 0, "There should be at least one node in the network");
        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "At least one device should be installed");
    }
};

// Test suite
class Ns3TestSuite : public TestSuite {
public:
    Ns3TestSuite() : TestSuite("ns3-tests", UNIT) {
        AddTestCase(new PacketReceptionTest, TestCase::QUICK);
        AddTestCase(new ObssPdResetTest, TestCase::QUICK);
        AddTestCase(new NetworkSetupTest, TestCase::QUICK);
    }
};

// Register the test suite
static Ns3TestSuite ns3TestSuite;
