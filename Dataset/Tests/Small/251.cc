#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

class RingTopologyTestSuite : public TestCase {
public:
    RingTopologyTestSuite() : TestCase("Test Ring Topology Setup") {}
    virtual ~RingTopologyTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestRingLinkCreation();
        TestLinkAttributes();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation() {
        uint32_t numNodes = 4;

        NodeContainer nodes;
        nodes.Create(numNodes);

        // Ensure that the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), numNodes, "Node creation failed: Expected 4 nodes.");
    }

    // Test the creation of links to form a ring topology
    void TestRingLinkCreation() {
        uint32_t numNodes = 4;

        NodeContainer nodes;
        nodes.Create(numNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Count the total number of links created
        uint32_t linkCount = 0;
        
        // Create links between consecutive nodes to form a ring topology
        for (uint32_t i = 0; i < numNodes; ++i) {
            uint32_t nextNode = (i + 1) % numNodes;
            NodeContainer link(nodes.Get(i), nodes.Get(nextNode));
            NetDeviceContainer devices = p2p.Install(link);
            linkCount++;
        }

        // Verify that the correct number of links are created in the ring network
        NS_TEST_ASSERT_MSG_EQ(linkCount, numNodes, "Ring link creation failed: Expected " << numNodes << " links.");
    }

    // Test that the link attributes (data rate and delay) are set correctly
    void TestLinkAttributes() {
        uint32_t numNodes = 4;

        NodeContainer nodes;
        nodes.Create(numNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install links and check link attributes
        for (uint32_t i = 0; i < numNodes; ++i) {
            uint32_t nextNode = (i + 1) % numNodes;
            NodeContainer link(nodes.Get(i), nodes.Get(nextNode));
            NetDeviceContainer devices = p2p.Install(link);

            // Check the data rate and delay for the link
            Ptr<PointToPointNetDevice> dev1 = devices.Get(0)->GetObject<PointToPointNetDevice>();
            Ptr<PointToPointNetDevice> dev2 = devices.Get(1)->GetObject<PointToPointNetDevice>();

            NS_TEST_ASSERT_MSG_EQ(dev1->GetDataRate(), DataRate("5Mbps"), "Data rate mismatch for link between Node " << i << " and Node " << nextNode);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetDataRate(), DataRate("5Mbps"), "Data rate mismatch for link between Node " << i << " and Node " << nextNode);

            NS_TEST_ASSERT_MSG_EQ(dev1->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link between Node " << i << " and Node " << nextNode);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link between Node " << i << " and Node " << nextNode);
        }
    }
};

// Instantiate the test suite
static RingTopologyTestSuite ringTopologyTestSuite;
