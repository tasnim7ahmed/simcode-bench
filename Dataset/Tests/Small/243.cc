#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

class NodeCreationTestSuite : public TestCase {
public:
    NodeCreationTestSuite() : TestCase("Test Node Creation Example") {}
    virtual ~NodeCreationTestSuite() {}

    void DoRun() override {
        TestNodeContainerCreation();
        TestNodeCount();
    }

private:
    // Test the creation of nodes in the NodeContainer
    void TestNodeContainerCreation() {
        NodeContainer nodes;
        nodes.Create(3);

        // Verify if the node container is not empty
        NS_TEST_ASSERT_MSG_GT(nodes.GetN(), 0, "Node container is empty, node creation failed.");
    }

    // Test if the correct number of nodes are created
    void TestNodeCount() {
        NodeContainer nodes;
        nodes.Create(3);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation count is incorrect.");
    }
};

// Instantiate the test suite
static NodeCreationTestSuite nodeCreationTestSuite;
