#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

class StarTopologyTestSuite : public TestCase {
public:
    StarTopologyTestSuite() : TestCase("Test Star Topology Setup") {}
    virtual ~StarTopologyTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestPointToPointLinkSetup();
        TestLinkAttributes();
    }

private:
    // Test the creation of central and peripheral nodes
    void TestNodeCreation() {
        uint32_t numPeripheralNodes = 4;

        NodeContainer centralNode;
        centralNode.Create(1);

        NodeContainer peripheralNodes;
        peripheralNodes.Create(numPeripheralNodes);

        // Ensure that 1 central node and 4 peripheral nodes are created
        NS_TEST_ASSERT_MSG_EQ(centralNode.GetN(), 1, "Central node creation failed: Expected 1 node.");
        NS_TEST_ASSERT_MSG_EQ(peripheralNodes.GetN(), numPeripheralNodes, "Peripheral nodes creation failed: Expected 4 nodes.");
    }

    // Test the creation of point-to-point links between central node and peripheral nodes
    void TestPointToPointLinkSetup() {
        uint32_t numPeripheralNodes = 4;

        NodeContainer centralNode;
        centralNode.Create(1);

        NodeContainer peripheralNodes;
        peripheralNodes.Create(numPeripheralNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install links from the central node to each peripheral node
        for (uint32_t i = 0; i < numPeripheralNodes; ++i) {
            NodeContainer link(centralNode.Get(0), peripheralNodes.Get(i));
            NetDeviceContainer devices = p2p.Install(link);
            
            // Ensure the devices are installed correctly
            NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install devices for link " << i);
        }
    }

    // Test that the link attributes are set correctly
    void TestLinkAttributes() {
        uint32_t numPeripheralNodes = 4;

        NodeContainer centralNode;
        centralNode.Create(1);

        NodeContainer peripheralNodes;
        peripheralNodes.Create(numPeripheralNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install links and verify attributes
        for (uint32_t i = 0; i < numPeripheralNodes; ++i) {
            NodeContainer link(centralNode.Get(0), peripheralNodes.Get(i));
            NetDeviceContainer devices = p2p.Install(link);

            // Check the data rate and delay for the link
            Ptr<PointToPointNetDevice> dev1 = devices.Get(0)->GetObject<PointToPointNetDevice>();
            Ptr<PointToPointNetDevice> dev2 = devices.Get(1)->GetObject<PointToPointNetDevice>();

            NS_TEST_ASSERT_MSG_EQ(dev1->GetDataRate(), DataRate("10Mbps"), "Data rate mismatch for link " << i);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetDataRate(), DataRate("10Mbps"), "Data rate mismatch for link " << i);

            NS_TEST_ASSERT_MSG_EQ(dev1->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link " << i);
            NS_TEST_ASSERT_MSG_EQ(dev2->GetChannel()->GetDelay(), Time("2ms"), "Delay mismatch for link " << i);
        }
    }
};

// Instantiate the test suite
static StarTopologyTestSuite starTopologyTestSuite;
