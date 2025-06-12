#include "ns3/test.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"

using namespace ns3;

class ErrorRateModelTest : public TestCase {
public:
    ErrorRateModelTest() : TestCase("ErrorRateModelTest") {}

    virtual void DoRun() {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
        WifiTxVector txVector;
        uint32_t frameSizeBits = 1500 * 8;

        const std::vector<std::string> modes = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};

        for (const auto& mode : modes) {
            txVector.SetMode(mode);
            WifiMode wifiMode(mode);

            for (double snrDb = -10.0; snrDb <= 20.0; snrDb += 1.0) {
                double snr = std::pow(10.0, snrDb / 10.0);

                double psYans = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
                double psNist = nist->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
                double psTable = table->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);

                NS_TEST_ASSERT_MSG(psYans >= 0.0 && psYans <= 1.0, "Invalid success rate for YansErrorRateModel");
                NS_TEST_ASSERT_MSG(psNist >= 0.0 && psNist <= 1.0, "Invalid success rate for NistErrorRateModel");
                NS_TEST_ASSERT_MSG(psTable >= 0.0 && psTable <= 1.0, "Invalid success rate for TableBasedErrorRateModel");
                
                NS_TEST_ASSERT_MSG(psYans == psNist, "Mismatch between Yans and Nist Error Rate Models");
                NS_TEST_ASSERT_MSG(psYans == psTable, "Mismatch between Yans and Table-based Error Rate Models");
            }
        }
    }
};

static ErrorRateModelTest g_errorRateModelTest;
