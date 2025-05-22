#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"
#include "ns3/vector.h"
#include "node-statistics.h"

using namespace ns3;

class NodeStatisticsTest : public TestCase {
public:
    NodeStatisticsTest() : TestCase("NodeStatistics Unit Test") {}

    virtual void DoRun() {
        Ptr<NodeStatistics> nodeStats = CreateObject<NodeStatistics>();
        Ptr<Node> node = CreateObject<Node>();
        nodeStats->SetNode(node);

        TestPowerAdaptation(nodeStats);
        TestRateAdaptation(nodeStats);
        TestPositionUpdate(nodeStats);
        TestThroughputCalculation(nodeStats);
    }

private:
    void TestPowerAdaptation(Ptr<NodeStatistics> nodeStats) {
        double initialPower = nodeStats->GetTransmissionPower();
        nodeStats->AdaptTransmissionPower();
        double updatedPower = nodeStats->GetTransmissionPower();
        NS_TEST_EXPECT_MSG_NE(initialPower, updatedPower, "Transmission power should change after adaptation.");
    }

    void TestRateAdaptation(Ptr<NodeStatistics> nodeStats) {
        double initialRate = nodeStats->GetTransmissionRate();
        nodeStats->AdaptTransmissionRate();
        double updatedRate = nodeStats->GetTransmissionRate();
        NS_TEST_EXPECT_MSG_NE(initialRate, updatedRate, "Transmission rate should change after adaptation.");
    }

    void TestPositionUpdate(Ptr<NodeStatistics> nodeStats) {
        Vector initialPosition = nodeStats->GetPosition();
        nodeStats->UpdatePosition(Vector(10.0, 20.0, 0.0));
        Vector updatedPosition = nodeStats->GetPosition();
        NS_TEST_EXPECT_MSG_EQ(updatedPosition, Vector(10.0, 20.0, 0.0), "Position should update correctly.");
    }

    void TestThroughputCalculation(Ptr<NodeStatistics> nodeStats) {
        nodeStats->RecordPacketTransmission(1000, Simulator::Now());
        nodeStats->RecordPacketTransmission(2000, Simulator::Now() + Seconds(1));
        double throughput = nodeStats->CalculateThroughput();
        NS_TEST_EXPECT_MSG_GT(throughput, 0, "Throughput should be positive.");
    }
};

static NodeStatisticsTest g_nodeStatisticsTest;
