#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

class LinearTopologyTestSuite : public TestCase {
public:
    LinearTopologyTestSuite() : TestCase("Test Linear Topology Setup") {}
    virtual ~LinearTopologyTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestLinkAttributes();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation() {
        uint32_t numNodes = 5;

        NodeContainer nodes;
        nodes.Create(numNodes);

        // Ensure that the correct number of nodes is created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), numNodes, "Node creation failed: Expected 5 nodes.");
    }

    // Test the creation of point-to-point links between consecutive nodes
    void TestPointToPointLinkSetup() {
        uint32_t numNodes = 5;

        NodeContainer nodes;
        nodes.Create(numNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install links between consecutive nodes and verify that devices are installed
        for (uint32_t i = 0; i < numNodes - 1; ++i) {
            NodeContainer link(nodes.Get(i), nodes.Get(i + 1));
            NetDeviceContainer devices = p2p.Install(link);
            
            // Ensure that devices are installed for each link
            NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install devices for link between Node " << i << " and Node " << i + 1);
        }
    }

    // Test that the link attributes (data rate and delay) are set correctly
    void TestLinkAttributes() {
        uint32_t numNodes = 5;

        NodeContainer nodes;
        nodes.Create(numNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install links and check link attributes
        for (uint32_t i = 0; i < numNodes - 1; ++i) {
            NodeContainer link(nodes.Get(i), nodes.Get(i + 1));
            NetDeviceContainer devices = p2p.Install(link);

            // Check the data rate and delay for the link
            Ptr<PointToPointNetDevice> dev1 = devices.Get(0)->GetObject<PointToPointNetDevice>();
            Ptr<PointToPointNetDevice> dev2 = devices.Get(1)->GetObject<PointToPointNetDevice>();

            NS_TEST_ASSERT_MSG_EQ(dev1->GetDataRate(), DataRate("5Mbps"), "Data rate mismatch for link between Node " << i << " and Node " << i + 1);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetDataRate(), DataRate("5Mbps"), "Data rate mismatch for link between Node " << i << " and Node " << i + 1);

            NS_TEST_ASSERT_MSG_EQ(dev1->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link between Node " << i << " and Node " << i + 1);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link between Node " << i << " and Node " << i + 1);
        }
    }
};

// Instantiate the test suite
static LinearTopologyTestSuite linearTopologyTestSuite;
