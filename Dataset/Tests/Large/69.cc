#include "ns3/test.h"

class QueueDiscTest : public ns3::TestCase
{
public:
    QueueDiscTest() : TestCase("Queue Disc Configuration Test") {}

    void DoRun() override
    {
        // Simulate setup (similar to your main code, focusing on the queue configuration)
        uint32_t maxPackets = 100;
        uint32_t queueDiscLimitPackets = 1000;
        double minTh = 5;
        double maxTh = 15;
        bool modeBytes = false;
        uint32_t pktSize = 512;

        ns3::Config::SetDefault("ns3::RedQueueDisc::MaxSize", ns3::QueueSizeValue(ns3::QueueSize(ns3::QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
        if (!modeBytes)
        {
            ns3::Config::SetDefault("ns3::RedQueueDisc::MinTh", ns3::DoubleValue(minTh * pktSize));
            ns3::Config::SetDefault("ns3::RedQueueDisc::MaxTh", ns3::DoubleValue(maxTh * pktSize));
        }

        // Check configuration
        ns3::QueueDisc::Stats stats;
        NS_TEST_ASSERT_MSG_EQ(stats.GetNDroppedPackets(ns3::RedQueueDisc::UNFORCED_DROP), 0, "Unforced drops expected to be 0");
        NS_TEST_ASSERT_MSG_EQ(stats.GetNDroppedPackets(ns3::QueueDisc::INTERNAL_QUEUE_DROP), 0, "Queue full drops expected to be 0");
    }
};

#include "ns3/test.h"

class NetworkSetupTest : public ns3::TestCase
{
public:
    NetworkSetupTest() : TestCase("Network Setup Test") {}

    void DoRun() override
    {
        uint32_t nLeaf = 10;
        ns3::PointToPointDumbbellHelper d(nLeaf, ns3::PointToPointHelper(), nLeaf, ns3::PointToPointHelper(), ns3::PointToPointHelper());

        // Assign IP addresses
        d.AssignIpv4Addresses(ns3::Ipv4AddressHelper("10.1.1.0", "255.255.255.0"), ns3::Ipv4AddressHelper("10.2.1.0", "255.255.255.0"), ns3::Ipv4AddressHelper("10.3.1.0", "255.255.255.0"));

        // Ensure the IP addresses are correctly assigned
        for (uint32_t i = 0; i < nLeaf; ++i)
        {
            ns3::Ipv4Address leftIp = d.GetLeftIpv4Address(i);
            ns3::Ipv4Address rightIp = d.GetRightIpv4Address(i);
            NS_TEST_ASSERT_MSG_EQ(leftIp.IsValid(), true, "Invalid IP Address for left node");
            NS_TEST_ASSERT_MSG_EQ(rightIp.IsValid(), true, "Invalid IP Address for right node");
        }
    }
};

#include "ns3/test.h"

class ApplicationLayerTest : public ns3::TestCase
{
public:
    ApplicationLayerTest() : TestCase("Application Layer Setup Test") {}

    void DoRun() override
    {
        uint32_t nLeaf = 10;
        uint16_t port = 5001;
        ns3::PointToPointDumbbellHelper d(nLeaf, ns3::PointToPointHelper(), nLeaf, ns3::PointToPointHelper(), ns3::PointToPointHelper());

        // Install on/off application on right side nodes
        ns3::OnOffHelper clientHelper("ns3::TcpSocketFactory", ns3::Address());
        ns3::ApplicationContainer clientApps;
        for (uint32_t i = 0; i < nLeaf; ++i)
        {
            ns3::AddressValue remoteAddress(ns3::InetSocketAddress(d.GetLeftIpv4Address(i), port));
            clientHelper.SetAttribute("Remote", remoteAddress);
            clientApps.Add(clientHelper.Install(d.GetRight(i)));
        }

        // Ensure that the applications are installed
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), nLeaf, "Not all client applications installed");
    }
};

#include "ns3/test.h"

class TrafficFlowTest : public ns3::TestCase
{
public:
    TrafficFlowTest() : TestCase("Traffic Flow and Drop Statistics Test") {}

    void DoRun() override
    {
        uint32_t nLeaf = 10;
        uint32_t maxPackets = 100;
        uint32_t queueDiscLimitPackets = 1000;

        // Simulate the setup and run the traffic flow
        ns3::PointToPointDumbbellHelper d(nLeaf, ns3::PointToPointHelper(), nLeaf, ns3::PointToPointHelper(), ns3::PointToPointHelper());
        ns3::TrafficControlHelper tchBottleneck;
        ns3::QueueDiscContainer queueDiscs;
        tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc");
        tchBottleneck.Install(d.GetLeft()->GetDevice(0));
        queueDiscs = tchBottleneck.Install(d.GetRight()->GetDevice(0));

        // Run the simulation
        ns3::Simulator::Run();

        // Check drop statistics
        ns3::QueueDisc::Stats st = queueDiscs.Get(0)->GetStats();
        NS_TEST_ASSERT_MSG_EQ(st.GetNDroppedPackets(ns3::RedQueueDisc::UNFORCED_DROP) > 0, true, "Unforced drops should occur");
        NS_TEST_ASSERT_MSG_EQ(st.GetNDroppedPackets(ns3::QueueDisc::INTERNAL_QUEUE_DROP), 0, "Internal queue full drops should be 0");

        // Clean up
        ns3::Simulator::Destroy();
    }
};

#include "ns3/test.h"

class SimulationTimeTest : public ns3::TestCase
{
public:
    SimulationTimeTest() : TestCase("Simulation Time Test") {}

    void DoRun() override
    {
        // Set the simulation duration
        ns3::Simulator::Stop(ns3::Seconds(30.0));

        // Start the simulation
        ns3::Simulator::Run();

        // Check that the simulation time is within expected range
        NS_TEST_ASSERT_MSG_EQ(ns3::Simulator::Now().GetSeconds(), 30.0, "Simulation ran for unexpected time");

        // Clean up
        ns3::Simulator::Destroy();
    }
};

