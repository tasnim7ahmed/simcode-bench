#include "ns3/core-module.h"
#include "ns3/test.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "experiment.h" // Ensure this header defines the Experiment class

using namespace ns3;

/**
 * Test case to check whether node position is correctly set and retrieved.
 */
class TestNodePosition : public TestCase
{
  public:
    TestNodePosition() : TestCase("Test node position") {}
    virtual void DoRun()
    {
        Ptr<Node> node = CreateObject<Node>();
        node->AggregateObject(CreateObject<ConstantPositionMobilityModel>());
        Experiment experiment;

        Vector position(10.0, 20.0, 0.0);
        experiment.SetPosition(node, position);
        Vector retrievedPosition = experiment.GetPosition(node);

        NS_TEST_ASSERT_MSG_EQ_TOL(retrievedPosition.x, 10.0, 0.01, "X position mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(retrievedPosition.y, 20.0, 0.01, "Y position mismatch");
    }
};

/**
 * Test case to check whether the packet reception increases received byte count.
 */
class TestPacketReception : public TestCase
{
  public:
    TestPacketReception() : TestCase("Test packet reception") {}
    virtual void DoRun()
    {
        Experiment experiment;
        Ptr<Node> node = CreateObject<Node>();
        Ptr<Socket> socket = experiment.SetupPacketReceive(node);

        uint32_t initialBytes = experiment.GetBytesTotal();

        Ptr<Packet> packet = Create<Packet>(1024);
        socket->Recv(packet);
        uint32_t newBytes = experiment.GetBytesTotal();

        NS_TEST_ASSERT_MSG_GT(newBytes, initialBytes, "Packet reception did not increase byte count");
    }
};

/**
 * Test case to check experiment execution.
 */
class TestExperimentRun : public TestCase
{
  public:
    TestExperimentRun() : TestCase("Test Experiment Run") {}
    virtual void DoRun()
    {
        Experiment experiment;
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a);
        WifiMacHelper wifiMac;
        YansWifiPhyHelper wifiPhy;
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiMac.SetType("ns3::AdhocWifiMac");
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate54Mbps"));

        Gnuplot2dDataset dataset = experiment.Run(wifi, wifiPhy, wifiMac, wifiChannel);

        NS_TEST_ASSERT_MSG_GT(dataset.Size(), 0, "Experiment did not generate dataset");
    }
};

/**
 * Main test suite containing all test cases.
 */
class ExperimentTestSuite : public TestSuite
{
  public:
    ExperimentTestSuite() : TestSuite("experiment", UNIT)
    {
        AddTestCase(new TestNodePosition, TestCase::QUICK);
        AddTestCase(new TestPacketReception, TestCase::QUICK);
        AddTestCase(new TestExperimentRun, TestCase::EXTENSIVE);
    }
};

static ExperimentTestSuite experimentTestSuite;
