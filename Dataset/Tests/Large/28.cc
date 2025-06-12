#include "ns3/test.h"
#include "ns3/experiment.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ExperimentTest");

class ExperimentTestCase : public TestCase {
public:
    ExperimentTestCase(std::string name, uint32_t mcs, uint32_t distance, double expectedMin, double expectedMax)
        : TestCase(name), m_mcs(mcs), m_distance(distance), m_expectedMin(expectedMin), m_expectedMax(expectedMax) {}

    virtual void DoRun(void) {
        Experiment exp;
        double throughput = exp.Run(m_mcs, m_distance);
        
        NS_TEST_ASSERT_MSG_GE(throughput, m_expectedMin, "Throughput is below expected minimum");
        NS_TEST_ASSERT_MSG_LE(throughput, m_expectedMax, "Throughput exceeds expected maximum");
    }

private:
    uint32_t m_mcs;
    uint32_t m_distance;
    double m_expectedMin;
    double m_expectedMax;
};

class ExperimentTestSuite : public TestSuite {
public:
    ExperimentTestSuite() : TestSuite("experiment", UNIT) {
        AddTestCase(new ExperimentTestCase("MCS 7, Distance 10m", 7, 10, 50.0, 100.0), TestCase::QUICK);
        AddTestCase(new ExperimentTestCase("MCS 3, Distance 50m", 3, 50, 20.0, 60.0), TestCase::QUICK);
        AddTestCase(new ExperimentTestCase("MCS 1, Distance 100m", 1, 100, 5.0, 30.0), TestCase::QUICK);
    }
};

static ExperimentTestSuite experimentTestSuite;
