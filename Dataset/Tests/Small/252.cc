#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

class TreeTopologyTestSuite : public TestCase {
public:
    TreeTopologyTestSuite() : TestCase("Test Tree Topology Setup") {}
    virtual ~TreeTopologyTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestTreeStructure();
        TestLinkAttributes();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation() {
        uint32_t numLevels = 2;
        uint32_t branchingFactor = 2;
        uint32_t numNodes = 1 + branchingFactor + (branchingFactor * branchingFactor); // Root + Level 1 + Level 2

        NodeContainer nodes;
        nodes.Create(numNodes);

        // Ensure that the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), numNodes, "Node creation failed: Expected " << numNodes << " nodes.");
    }

    // Test the creation of tree structure (root connected to first level, and first level to second level)
    void TestTreeStructure() {
        uint32_t numLevels = 2;
        uint32_t branchingFactor = 2;
        uint32_t numNodes = 1 + branchingFactor + (branchingFactor * branchingFactor); // Root + Level 1 + Level 2

        NodeContainer nodes;
        nodes.Create(numNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        uint32_t nodeIndex = 1;
        uint32_t parentIndex = 1;
        uint32_t linkCount = 0;

        // Connect root to first level nodes
        for (uint32_t i = 0; i < branchingFactor; ++i) {
            NodeContainer link(nodes.Get(0), nodes.Get(nodeIndex));
            p2p.Install(link);
            linkCount++;
            nodeIndex++;
        }

        // Connect first level nodes to second level nodes
        for (uint32_t i = 0; i < branchingFactor; ++i) {
            for (uint32_t j = 0; j < branchingFactor; ++j) {
                NodeContainer link(nodes.Get(parentIndex), nodes.Get(nodeIndex));
                p2p.Install(link);
                linkCount++;
                nodeIndex++;
            }
            parentIndex++;
        }

        // Verify that the correct number of links are created (root to level 1, level 1 to level 2)
        NS_TEST_ASSERT_MSG_EQ(linkCount, numNodes - 1, "Tree structure failed: Expected " << numNodes - 1 << " links.");
    }

    // Test that the link attributes (data rate and delay) are set correctly
    void TestLinkAttributes() {
        uint32_t numLevels = 2;
        uint32_t branchingFactor = 2;
        uint32_t numNodes = 1 + branchingFactor + (branchingFactor * branchingFactor); // Root + Level 1 + Level 2

        NodeContainer nodes;
        nodes.Create(numNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        uint32_t nodeIndex = 1;
        uint32_t parentIndex = 1;

        // Check link attributes for root to first level nodes
        for (uint32_t i = 0; i < branchingFactor; ++i) {
            NodeContainer link(nodes.Get(0), nodes.Get(nodeIndex));
            NetDeviceContainer devices = p2p.Install(link);
            nodeIndex++;

            Ptr<PointToPointNetDevice> dev1 = devices.Get(0)->GetObject<PointToPointNetDevice>();
            Ptr<PointToPointNetDevice> dev2 = devices.Get(1)->GetObject<PointToPointNetDevice>();

            // Check that the data rate and delay are set correctly
            NS_TEST_ASSERT_MSG_EQ(dev1->GetDataRate(), DataRate("10Mbps"), "Data rate mismatch for link between Root and Node " << i);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetDataRate(), DataRate("10Mbps"), "Data rate mismatch for link between Root and Node " << i);

            NS_TEST_ASSERT_MSG_EQ(dev1->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link between Root and Node " << i);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link between Root and Node " << i);
        }

        // Check link attributes for first level to second level nodes
        for (uint32_t i = 0; i < branchingFactor; ++i) {
            for (uint32_t j = 0; j < branchingFactor; ++j) {
                NodeContainer link(nodes.Get(parentIndex), nodes.Get(nodeIndex));
                NetDeviceContainer devices = p2p.Install(link);
                nodeIndex++;

                Ptr<PointToPointNetDevice> dev1 = devices.Get(0)->GetObject<PointToPointNetDevice>();
                Ptr<PointToPointNetDevice> dev2 = devices.Get(1)->GetObject<PointToPointNetDevice>();

                // Check that the data rate and delay are set correctly
                NS_TEST_ASSERT_MSG_EQ(dev1->GetDataRate(), DataRate("10Mbps"), "Data rate mismatch for link between Node " << parentIndex << " and Node " << nodeIndex);
                NS_TEST_ASSERT_MSG_EQ(dev2->GetDataRate(), DataRate("10Mbps"), "Data rate mismatch for link between Node " << parentIndex << " and Node " << nodeIndex);

                NS_TEST_ASSERT_MSG_EQ(dev1->GetChannel
