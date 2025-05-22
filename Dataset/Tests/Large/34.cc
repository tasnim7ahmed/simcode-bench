#include "ns3/test.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/yans-error-rate-model.h"
#include <cmath>

using namespace ns3;

class ErrorRateModelTestCase : public TestCase
{
public:
    ErrorRateModelTestCase() : TestCase("Error rate model unit tests") {}

    void DoRun() override
    {
        TestModelInstantiation();
        TestChunkSuccessRateBounds();
        TestDifferentModes();
    }

private:
    void TestModelInstantiation()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        NS_TEST_ASSERT_MSG_NE(yans, nullptr, "YansErrorRateModel instantiation failed");
        NS_TEST_ASSERT_MSG_NE(nist, nullptr, "NistErrorRateModel instantiation failed");
        NS_TEST_ASSERT_MSG_NE(table, nullptr, "TableBasedErrorRateModel instantiation failed");
    }

    void TestChunkSuccessRateBounds()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        WifiTxVector txVector;
        txVector.SetMode("HeMcs0");
        WifiMode wifiMode("HeMcs0");

        uint32_t frameSizeBits = 1500 * 8;
        for (double snrDb = -5.0; snrDb <= 40.0; snrDb += 5.0)
        {
            double snr = std::pow(10.0, snrDb / 10.0);

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
    }

    void TestDifferentModes()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        WifiTxVector txVector;
        uint32_t frameSizeBits = 1500 * 8;
        const std::vector<std::string> modes{
            "HeMcs0", "HeMcs1", "HeMcs2", "HeMcs3",
            "HeMcs4", "HeMcs5", "HeMcs6", "HeMcs7",
            "HeMcs8", "HeMcs9", "HeMcs10", "HeMcs11"};

        for (const auto& mode : modes)
        {
            txVector.SetMode(mode);
            WifiMode wifiMode(mode);
            double snr = std::pow(10.0, 10.0 / 10.0); // SNR = 10 dB

            double ps = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
            NS_TEST_ASSERT_MSG_GE(ps, 0.0, "Probability is below 0 for mode " + mode);
            NS_TEST_ASSERT_MSG_LE(ps, 1.0, "Probability is above 1 for mode " + mode);
        }
    }
};

// Register the test case
class ErrorRateModelTestSuite : public TestSuite
{
public:
    ErrorRateModelTestSuite() : TestSuite("error-rate-model-tests", UNIT)
    {
        AddTestCase(new ErrorRateModelTestCase, TestCase::QUICK);
    }
};

static ErrorRateModelTestSuite errorRateModelTestSuite;
