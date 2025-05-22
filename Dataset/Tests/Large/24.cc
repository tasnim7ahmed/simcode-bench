#include "ns3/test.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"

#include <cmath>

using namespace ns3;

class ErrorRateModelTest : public TestCase
{
public:
    ErrorRateModelTest() : TestCase("Error rate model unit tests") {}

    void TestProbabilityBounds()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
        WifiTxVector txVector;
        uint32_t size = 1500 * 8; // bits

        std::vector<std::string> modes = {"HtMcs0", "HtMcs4", "HtMcs7"};

        for (const auto& mode : modes)
        {
            WifiMode wifiMode(mode);
            txVector.SetMode(mode);

            for (double snrDb = -5.0; snrDb <= 35.0; snrDb += 5.0)
            {
                double snr = std::pow(10.0, snrDb / 10.0);
                
                double psYans = yans->GetChunkSuccessRate(wifiMode, txVector, snr, size);
                double psNist = nist->GetChunkSuccessRate(wifiMode, txVector, snr, size);
                double psTable = table->GetChunkSuccessRate(wifiMode, txVector, snr, size);

                NS_TEST_ASSERT_MSG(psYans >= 0.0 && psYans <= 1.0, "Yans model returned an invalid probability");
                NS_TEST_ASSERT_MSG(psNist >= 0.0 && psNist <= 1.0, "Nist model returned an invalid probability");
                NS_TEST_ASSERT_MSG(psTable >= 0.0 && psTable <= 1.0, "Table model returned an invalid probability");
            }
        }
    }

    void TestConsistencyAcrossModels()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
        WifiTxVector txVector;
        uint32_t size = 1500 * 8;

        std::vector<std::string> modes = {"HtMcs0", "HtMcs4", "HtMcs7"};

        for (const auto& mode : modes)
        {
            WifiMode wifiMode(mode);
            txVector.SetMode(mode);

            for (double snrDb = -5.0; snrDb <= 35.0; snrDb += 5.0)
            {
                double snr = std::pow(10.0, snrDb / 10.0);

                double psYans = yans->GetChunkSuccessRate(wifiMode, txVector, snr, size);
                double psNist = nist->GetChunkSuccessRate(wifiMode, txVector, snr, size);
                double psTable = table->GetChunkSuccessRate(wifiMode, txVector, snr, size);

                NS_TEST_ASSERT_MSG(std::abs(psYans - psNist) < 0.1, "Yans and Nist models differ significantly");
                NS_TEST_ASSERT_MSG(std::abs(psYans - psTable) < 0.1, "Yans and Table models differ significantly");
            }
        }
    }

    virtual void DoRun()
    {
        TestProbabilityBounds();
        TestConsistencyAcrossModels();
    }
};

static ErrorRateModelTest errorRateModelTest;
