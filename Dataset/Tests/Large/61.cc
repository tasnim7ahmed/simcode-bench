#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-disc.h"
#include "ns3/log.h"

using namespace ns3;

class QueueSizeTest : public TestCase
{
public:
    QueueSizeTest() : TestCase("Test Queue Size Check Function") {}

    void DoRun() override
    {
        // Create a QueueDisc
        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::FifoQueueDisc");
        Ptr<QueueDisc> qDisc = tch.Install(CreateObject<Node>()->GetDevice(0));

        // Test the queue size check callback
        Simulator::Schedule(Seconds(1.0), &CheckQueueSize, qDisc);

        // Run the simulation for a short time
        Simulator::Run();
        Simulator::Destroy();
        
        // Verify the queue size output in the file (simulating the file reading logic)
        // This step would require a mock or checking the actual contents of the output file
        NS_TEST_EXPECT_MSG_EQ(qDisc->GetCurrentSize().GetValue(), 0, "Queue size mismatch");
    }
};

TestSuite* CreateTestSuite()
{
    TestSuite* suite = new TestSuite("QueueSizeTest", TEST_SUITE);
    suite->AddTestCase(new QueueSizeTest());
    return suite;
}
