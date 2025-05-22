#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

class NodeIDTestSuite : public TestCase {
public:
    NodeIDTestSuite() : TestCase("Test Node ID Retrieval") {}
    virtual ~NodeIDTestSuite() {}

    void DoRun() override {
        TestNodeContainerCreation();
        TestNodeIDAssignment();
        TestNodeIDLogging();
    }

private:
    // Test the creation of the NodeContainer
    void TestNodeContainerCreation() {
        NodeContainer nodes;
        nodes.Create(5);

        // Verify if the node container is not empty
        NS_TEST_ASSERT_MSG_GT(nodes.GetN(), 0, "Node container is empty, node creation failed.");
    }

    // Test if node IDs are assigned correctly
    void TestNodeIDAssignment() {
        NodeContainer nodes;
        nodes.Create(5);

        // Verify that node IDs are correctly assigned (IDs start at 0)
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            NS_TEST_ASSERT_MSG_EQ(node->GetId(), i, "Node ID mismatch: Node " << i << " should have ID " << i << " but has ID " << node->GetId());
        }
    }

    // Test that the node IDs are correctly logged
    void TestNodeIDLogging() {
        NodeContainer nodes;
        nodes.Create(5);
        
        // Create a string stream to capture logs
        std::ostringstream logStream;

        // Redirect the output log to capture the printed node IDs
        LogComponentEnable("NodeIDExample", LOG_LEVEL_INFO);
        NS_LOG_INFO("Logging node IDs for verification...");

        // Capture the log outputs
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            logStream << "Node " << i << " has ID: " << node->GetId() << std::endl;
        }

        // Validate the log stream contents by checking if they match the expected format
        std::string expectedLog = "Logging node IDs for verification...";
        expectedLog += "\nNode 0 has ID: 0\n";
        expectedLog += "Node 1 has ID: 1\n";
        expectedLog += "Node 2 has ID: 2\n";
        expectedLog += "Node 3 has ID: 3\n";
        expectedLog += "Node 4 has ID: 4\n";

        NS_TEST_ASSERT_MSG_EQ(logStream.str(), expectedLog, "Node IDs were not logged correctly.");
    }
};

// Instantiate the test suite
static NodeIDTestSuite nodeIDTestSuite;
