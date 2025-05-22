#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

class ThroughputTestCase : public TestCase {
public:
    ThroughputTestCase() : TestCase("Throughput Calculation Test") {}
    virtual void DoRun() {
        double receivedBits = 8000000; // 8 Mbps in bits
        double timeInterval = 1.0;     // 1 second
        double expectedThroughput = receivedBits / (timeInterval * 1e6); // Mbps
        double actualThroughput = receivedBits / (timeInterval * 1e6);
        NS_TEST_ASSERT_MSG_EQ_TOL(actualThroughput, expectedThroughput, 0.001, "Throughput calculation incorrect");
    }
};

class FairnessIndexTestCase : public TestCase {
public:
    FairnessIndexTestCase() : TestCase("Fairness Index Calculation Test") {}
    virtual void DoRun() {
        std::vector<double> throughputs = {1.0, 1.5, 2.0};
        double numerator = std::pow(std::accumulate(throughputs.begin(), throughputs.end(), 0.0), 2);
        double denominator = throughputs.size() * std::accumulate(throughputs.begin(), throughputs.end(), 0.0, [](double a, double b) { return a + b * b; });
        double expectedFairnessIndex = numerator / denominator;
        NS_TEST_ASSERT_MSG_EQ_TOL(expectedFairnessIndex, 0.96, 0.01, "Fairness Index calculation incorrect");
    }
};

class QueueSizeTestCase : public TestCase {
public:
    QueueSizeTestCase() : TestCase("Queue Size Tracking Test") {}
    virtual void DoRun() {
        Ptr<Queue<Packet>> queue = CreateObject<DropTailQueue<Packet>>();
        queue->SetMaxSize(QueueSize("10p"));
        queue->Enqueue(Create<Packet>(100));
        NS_TEST_ASSERT_MSG_EQ(queue->GetNPackets(), 1, "Queue size tracking incorrect");
    }
};

class PacketCountTestCase : public TestCase {
public:
    PacketCountTestCase() : TestCase("Packet Count Test") {}
    virtual void DoRun() {
        uint32_t packetCount = 0;
        auto PacketTrace = [&](Ptr<const Packet>) { packetCount++; };
        Simulator::Schedule(Seconds(1), PacketTrace, Create<Packet>(100));
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(packetCount, 1, "Packet count tracking incorrect");
    }
};

class Ns3TestSuite : public TestSuite {
public:
    Ns3TestSuite() : TestSuite("ns3-unit-tests", UNIT) {
        AddTestCase(new ThroughputTestCase(), TestCase::QUICK);
        AddTestCase(new FairnessIndexTestCase(), TestCase::QUICK);
        AddTestCase(new QueueSizeTestCase(), TestCase::QUICK);
        AddTestCase(new PacketCountTestCase(), TestCase::QUICK);
    }
};

static Ns3TestSuite ns3TestSuiteInstance;
