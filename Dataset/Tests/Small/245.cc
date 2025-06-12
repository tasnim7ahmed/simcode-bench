#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/object.h"
#include "ns3/names-module.h"

using namespace ns3;

class NodeNamingTestSuite : public TestCase {
public:
    NodeNamingTestSuite() : TestCase("Test Node Naming Functionality") {}
    virtual ~NodeNamingTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestNodeNaming();
        TestNodeLogging();
    }

private:
    // Test the creation of nodes in NodeContainer
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(3);

        // Ensure the container has 3 nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation failed: Expected 3 nodes.");
    }

    // Test node naming using Object Name Service (ONS)
    void TestNodeNaming() {
        NodeContainer nodes;
        nodes.Create(3);

        // Assign names to the nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            std::ostringstream nodeName;
            nodeName << "Node-" << i;
            Names::Add(nodeName.str(), nodes.Get(i));

            // Verify that the name matches the expected name
            Ptr<Node> node = nodes.Get(i);
            std::string assignedName = Names::FindName(node);

            NS_TEST_ASSERT_MSG_EQ(assignedName, nodeName.str(), "Node name mismatch: Expected " << nodeName.str() << " but got " << assignedName);
        }
    }

    // Test that the node names and IDs are correctly logged
    void TestNodeLogging() {
        NodeContainer nodes;
        nodes.Create(3);

        // Create a string stream to capture logs
        std::ostringstream logStream;

        // Redirect output log to capture node name and IDs
        LogComponentEnable("NodeNamingExample", LOG_LEVEL_INFO);
        NS_LOG_INFO("Logging node names and IDs for verification...");

        // Capture the log outputs for each node
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            std::ostringstream nodeName;
            nodeName << "Node-" << i;
            Names::Add(nodeName.str(), nodes.Get(i));

            Ptr<Node> node = nodes.Get(i);
            logStream << nodeName.str() << " has ID: " << node->GetId() << std::endl;
        }

        // Validate the log stream contents by checking if they match the expected format
        std::string expectedLog = "Logging node names and IDs for verification...\n";
        expectedLog += "Node-0 has ID: 0\n";
        expectedLog += "Node-1 has ID: 1\n";
        expectedLog += "Node-2 has ID: 2\n";

        NS_TEST_ASSERT_MSG_EQ(logStream.str(), expectedLog, "Node names and IDs were not logged correctly.");
    }
};

// Instantiate the test suite
static NodeNamingTestSuite nodeNamingTestSuite;
