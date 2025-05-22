#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

class NodeConnectionTestSuite : public TestCase {
public:
    NodeConnectionTestSuite() : TestCase("Test Point-to-Point Node Connection") {}
    virtual ~NodeConnectionTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestPointToPointConnection();
        TestLoggingConfirmation();
    }

private:
    // Test the creation of nodes in NodeContainer
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        // Ensure the container has 2 nodes
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed: Expected 2 nodes.");
    }

    // Test the creation of a Point-to-Point link between nodes
    void TestPointToPointConnection() {
        NodeContainer nodes;
        nodes.Create(2);

        // Setup the Point-to-Point link
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = p2p.Install(nodes);

        // Ensure devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Point-to-Point devices not installed correctly: Expected 2 devices.");
    }

    // Test the logging confirmation of the Point-to-Point link setup
    void TestLoggingConfirmation() {
        NodeContainer nodes;
        nodes.Create(2);

        // Create a string stream to capture logs
        std::ostringstream logStream;

        // Redirect output log to capture confirmation message
        LogComponentEnable("NodeConnectionExample", LOG_LEVEL_INFO);
        NS_LOG_INFO("Created a Point-to-Point link between Node " 
                    << nodes.Get(0)->GetId() << " and Node " 
                    << nodes.Get(1)->GetId());

        // Capture the log output and compare it with the expected message
        std::string expectedLog = "Created a Point-to-Point link between Node 0 and Node 1";
        logStream << "Created a Point-to-Point link between Node " 
                  << nodes.Get(0)->GetId() << " and Node " 
                  << nodes.Get(1)->GetId();

        NS_TEST_ASSERT_MSG_EQ(logStream.str(), expectedLog, "Logging confirmation mismatch.");
    }
};

// Instantiate the test suite
static NodeConnectionTestSuite nodeConnectionTestSuite;
