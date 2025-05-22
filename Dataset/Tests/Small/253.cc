#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

class BusTopologyTestSuite : public TestCase {
public:
    BusTopologyTestSuite() : TestCase("Test Bus Topology Setup") {}
    virtual ~BusTopologyTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestBusTopologySetup();
        TestCSMAChannelAttributes();
    }

private:
    // Test the creation of nodes
    void TestNodeCreation() {
        uint32_t numNodes = 5;

        NodeContainer nodes;
        nodes.Create(numNodes);

        // Ensure that the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), numNodes, "Node creation failed: Expected " << numNodes << " nodes.");
    }

    // Test the bus topology setup (all nodes should be connected to the CSMA channel)
    void TestBusTopologySetup() {
        uint32_t numNodes = 5;

        NodeContainer nodes;
        nodes.Create(numNodes);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5)));

        NetDeviceContainer devices = csma.Install(nodes);

        // Verify that all nodes are connected to the shared CSMA channel
        for (uint32_t i = 0; i < numNodes; ++i) {
            NS_TEST_ASSERT_MSG_EQ(devices.Get(i)->GetObject<CsmaNetDevice>(), nullptr, "Device " << i << " failed to connect to the CSMA channel.");
        }
    }

    // Test CSMA channel attributes (Data rate and Delay)
    void TestCSMAChannelAttributes() {
        uint32_t numNodes = 5;

        NodeContainer nodes;
        nodes.Create(numNodes);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5)));

        NetDeviceContainer devices = csma.Install(nodes);

        // Check data rate and delay for all devices
        for (uint32_t i = 0; i < numNodes; ++i) {
            Ptr<CsmaNetDevice> csmaDevice = devices.Get(i)->GetObject<CsmaNetDevice>();
            Ptr<PointToPointChannel> channel = csmaDevice->GetChannel();

            // Check that the data rate is correct
            NS_TEST_ASSERT_MSG_EQ(channel->GetDataRate(), DataRate("100Mbps"), "Data rate mismatch for device " << i);

            // Check that the delay is correct
            NS_TEST_ASSERT_MSG_EQ(channel->GetDelay(), NanoSeconds(5), "Delay mismatch for device " << i);
        }
    }
};

// Instantiate the test suite
static BusTopologyTestSuite busTopologyTestSuite;
