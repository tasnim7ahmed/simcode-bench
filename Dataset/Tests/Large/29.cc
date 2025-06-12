#include "ns3/core-module.h"
#include "ns3/test.h"
#include "experiment.h"

using namespace ns3;

class ExperimentTest : public TestCase {
public:
    ExperimentTest() : TestCase("Experiment Unit Test") {}

    virtual void DoRun() {
        TestExperimentInitialization();
        TestRoutingFlag();
        TestMobilityFlag();
        TestScenarioNumber();
        TestRtsThreshold();
        TestOutputFileName();
        TestRateManager();
    }

    void TestExperimentInitialization() {
        Experiment exp("test");
        NS_TEST_ASSERT_MSG_EQ(exp.GetScenario(), 4, "Default scenario should be 4");
        NS_TEST_ASSERT_MSG_EQ(exp.GetRtsThreshold(), "2200", "Default RTS threshold should be 2200");
    }

    void TestRoutingFlag() {
        Experiment exp;
        NS_TEST_ASSERT_MSG_EQ(exp.IsRouting(), false, "Routing should be disabled by default");
    }

    void TestMobilityFlag() {
        Experiment exp;
        NS_TEST_ASSERT_MSG_EQ(exp.IsMobility(), false, "Mobility should be disabled by default");
    }

    void TestScenarioNumber() {
        Experiment exp;
        NS_TEST_ASSERT_MSG_EQ(exp.GetScenario(), 4, "Default scenario should be 4");
    }

    void TestRtsThreshold() {
        Experiment exp;
        NS_TEST_ASSERT_MSG_EQ(exp.GetRtsThreshold(), "2200", "Default RTS threshold should be 2200");
    }

    void TestOutputFileName() {
        Experiment exp;
        NS_TEST_ASSERT_MSG_EQ(exp.GetOutputFileName(), "minstrel", "Default output file name should be minstrel");
    }

    void TestRateManager() {
        Experiment exp;
        NS_TEST_ASSERT_MSG_EQ(exp.GetRateManager(), "ns3::MinstrelWifiManager", "Default rate manager should be ns3::MinstrelWifiManager");
    }
};

static ExperimentTest experimentTestInstance;
