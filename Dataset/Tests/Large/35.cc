#include "ns3/test.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/yans-error-rate-model.h"
#include <cmath>
#include <fstream>

using namespace ns3;

class ErrorRateModelTestCase : public TestCase
{
public:
    ErrorRateModelTestCase() : TestCase("Error rate model unit tests") {}

    void DoRun() override
    {
        TestModelInstantiation();
        TestChunkSuccessRateBounds();
        TestMultipleModes();
    }

private:
    void TestModelInstantiation()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        NS_TEST_ASSERT_MSG_NE(yans, nullptr, "Failed to create YansErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(nist, nullptr, "Failed to create NistErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(table, nullptr, "Failed to create TableBasedErrorRateModel");
    }

    void TestChunkSuccessRateBounds()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        WifiTxVector txVector;
        txVector.SetMode("OfdmRate6Mbps");
        WifiMode wifiMode("OfdmRate6Mbps");
        uint32_t frameSizeBits = 1500 * 8;
        double snr = std::pow(10.0, 10.0 / 10.0); // 10 dB SNR

        double psYans = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
        double psNist = nist->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
        double psTable = table->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);

        NS_TEST_ASSERT_MSG_GE(psYans, 0.0, "Yans model probability is below 0");
        NS_TEST_ASSERT_MSG_LE(psYans, 1.0, "Yans model probability is above 1");
        
        NS_TEST_ASSERT_MSG_GE(psNist, 0.0, "Nist model probability is below 0");
        NS_TEST_ASSERT_MSG_LE(psNist, 1.0, "Nist model probability is above 1");
        
        NS_TEST_ASSERT_MSG_GE(psTable, 0.0, "Table model probability is below 0");
        NS_TEST_ASSERT_MSG_LE(psTable, 1.0, "Table model probability is above 1");
    }

    void TestMultipleModes()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        WifiTxVector txVector;
        uint32_t frameSizeBits = 1500 * 8;
        const std::vector<std::string> modes = {
            "OfdmRate6Mbps", "OfdmRate9Mbps", "OfdmRate12Mbps", "OfdmRate18Mbps",
            "OfdmRate24Mbps", "OfdmRate36Mbps", "OfdmRate48Mbps", "OfdmRate54Mbps"
        };
        
        for (const auto& mode : modes)
        {
            txVector.SetMode(mode);
            WifiMode wifiMode(mode);
            double snr = std::pow(10.0, 10.0 / 10.0);

            double ps = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
            NS_TEST_ASSERT_MSG_GE(ps, 0.0, "Probability below 0 for mode " + mode);
            NS_TEST_ASSERT_MSG_LE(ps, 1.0, "Probability above 1 for mode " + mode);
        }
    }
};

class ErrorRateModelTestSuite : public TestSuite
{
public:
    ErrorRateModelTestSuite() : TestSuite("error-rate-model-tests", UNIT)
    {
        AddTestCase(new ErrorRateModelTestCase, TestCase::QUICK);
    }
};

static ErrorRateModelTestSuite errorRateModelTestSuite;
