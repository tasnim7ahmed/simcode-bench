#include "ns3/test.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"

#include <cmath>
#include <vector>

using namespace ns3;

/**
 * Test Suite for Error Rate Models (Yans, Nist, Table-based)
 */
class ErrorRateModelTestSuite : public TestSuite
{
public:
    ErrorRateModelTestSuite() : TestSuite("error-rate-model-tests", UNIT)
    {
        AddTestCase(new ModelInstantiationTestCase, TestCase::QUICK);
        AddTestCase(new SuccessRateBoundsTestCase, TestCase::QUICK);
        AddTestCase(new ModeHandlingTestCase, TestCase::QUICK);
    }
};

/**
 * Test Case: Ensure Models are Instantiated Correctly
 */
class ModelInstantiationTestCase : public TestCase
{
public:
    ModelInstantiationTestCase() : TestCase("Model Instantiation Test") {}

    virtual void DoRun()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        NS_TEST_ASSERT_MSG_NE(yans, nullptr, "Failed to instantiate YansErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(nist, nullptr, "Failed to instantiate NistErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(table, nullptr, "Failed to instantiate TableBasedErrorRateModel");
    }
};

/**
 * Test Case: Verify that the Success Rate is within Valid Bounds (0 <= ps <= 1)
 */
class SuccessRateBoundsTestCase : public TestCase
{
public:
    SuccessRateBoundsTestCase() : TestCase("Success Rate Bounds Test") {}

    virtual void DoRun()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        WifiTxVector txVector;
        WifiMode wifiMode("VhtMcs3"); // Test one mode
        uint32_t frameSizeBits = 1500 * 8;

        for (double snrDb = -5.0; snrDb <= 30.0; snrDb += 5.0)
        {
            double snr = std::pow(10.0, snrDb / 10.0);
            
            double ps_yans = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
            double ps_nist = nist->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
            double ps_table = table->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);

            NS_TEST_ASSERT_MSG_GE(ps_yans, 0.0, "Yans success rate out of bounds");
            NS_TEST_ASSERT_MSG_LE(ps_yans, 1.0, "Yans success rate out of bounds");

            NS_TEST_ASSERT_MSG_GE(ps_nist, 0.0, "Nist success rate out of bounds");
            NS_TEST_ASSERT_MSG_LE(ps_nist, 1.0, "Nist success rate out of bounds");

            NS_TEST_ASSERT_MSG_GE(ps_table, 0.0, "Table success rate out of bounds");
            NS_TEST_ASSERT_MSG_LE(ps_table, 1.0, "Table success rate out of bounds");
        }
    }
};

/**
 * Test Case: Validate that All VHT MCS Modes Work Correctly
 */
class ModeHandlingTestCase : public TestCase
{
public:
    ModeHandlingTestCase() : TestCase("Mode Handling Test") {}

    virtual void DoRun()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        const std::vector<std::string> modes{
            "VhtMcs0", "VhtMcs1", "VhtMcs2", "VhtMcs3", "VhtMcs4",
            "VhtMcs5", "VhtMcs6", "VhtMcs7", "VhtMcs8"
        };

        WifiTxVector txVector;
        uint32_t frameSizeBits = 1500 * 8;
        double snr = std::pow(10.0, 10.0 / 10.0); // Fixed SNR for mode test

        for (const auto &mode : modes)
        {
            WifiMode wifiMode(mode);
            txVector.SetMode(mode);

            double ps_yans = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
            double ps_nist = nist->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
            double ps_table = table->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);

            NS_TEST_ASSERT_MSG_GE(ps_yans, 0.0, "Yans failed for mode: " + mode);
            NS_TEST_ASSERT_MSG_LE(ps_yans, 1.0, "Yans failed for mode: " + mode);

            NS_TEST_ASSERT_MSG_GE(ps_nist, 0.0, "Nist failed for mode: " + mode);
            NS_TEST_ASSERT_MSG_LE(ps_nist, 1.0, "Nist failed for mode: " + mode);

            NS_TEST_ASSERT_MSG_GE(ps_table, 0.0, "Table failed for mode: " + mode);
            NS_TEST_ASSERT_MSG_LE(ps_table, 1.0, "Table failed for mode: " + mode);
        }
    }
};

// Register the test suite
static ErrorRateModelTestSuite errorRateModelTestSuite;
