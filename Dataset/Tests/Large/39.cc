#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/node-container.h"
#include "ns3/ipv4.h"
#include "ns3/net-device-container.h"
#include "ns3/packet.h"
#include "ns3/position-allocator.h"
#include "ns3/simulator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeStatisticsTest");

class NodeStatisticsTest : public TestCase {
public:
  NodeStatisticsTest() : TestCase("NodeStatistics Functionality Test") {}

  virtual void DoRun() {
    // Create a node
    Ptr<Node> node = CreateObject<Node>();

    // Assign a mobility model
    Ptr<ConstantPositionMobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
    node->AggregateObject(mobility);
    mobility->SetPosition(Vector(10.0, 20.0, 0.0));

    // Check initial position
    Vector pos = mobility->GetPosition();
    NS_TEST_ASSERT_MSG_EQ_TOL(pos.x, 10.0, 0.01, "Incorrect X position");
    NS_TEST_ASSERT_MSG_EQ_TOL(pos.y, 20.0, 0.01, "Incorrect Y position");

    // Simulate reception of a packet
    Ptr<Packet> packet = Create<Packet>(100); // 100-byte packet
    uint64_t initialDataRx = 0;
    uint64_t newDataRx = initialDataRx + packet->GetSize();
    NS_TEST_ASSERT_MSG_EQ(newDataRx, 100, "Packet reception failed");

    // Throughput calculation test
    double startTime = 1.0;
    double endTime = 3.0;
    double throughput = (newDataRx * 8) / ((endTime - startTime) * 1e6);
    NS_TEST_ASSERT_MSG_EQ_TOL(throughput, 0.4, 0.01, "Incorrect throughput calculation");
  }
};

static NodeStatisticsTest g_nodeStatisticsTest;
