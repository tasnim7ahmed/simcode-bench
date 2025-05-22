#include "ns3/test.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"
#include <cmath>

using namespace ns3;

class ErrorRateModelTest : public TestCase {
public:
    ErrorRateModelTest() : TestCase("Error Rate Model Test") {}
    
    virtual void DoRun() {
        TestErrorRateModelsInstantiation();
        TestChunkSuccessRateBounds();
    }

    void TestErrorRateModelsInstantiation() {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
        
        NS_TEST_ASSERT_MSG_NE(yans, nullptr, "Failed to instantiate YansErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(nist, nullptr, "Failed to instantiate NistErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(table, nullptr, "Failed to instantiate TableBasedErrorRateModel");
    }

    void TestChunkSuccessRateBounds() {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
        
        WifiTxVector txVector;
        txVector.SetMode("HtMcs0");
        WifiMode wifiMode("HtMcs0");
        
        double snrDb = 10.0;
        double snr = std::pow(10.0, snrDb / 10.0);
        uint32_t frameSizeBits = 1500 * 8;
        
        double psYans = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
        double psNist = nist->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
        double psTable = table->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
        
        NS_TEST_ASSERT_MSG_GE(psYans, 0.0, "Yans model returned probability < 0");
        NS_TEST_ASSERT_MSG_LE(psYans, 1.0, "Yans model returned probability > 1");
        
        NS_TEST_ASSERT_MSG_GE(psNist, 0.0, "Nist model returned probability < 0");
        NS_TEST_ASSERT_MSG_LE(psNist, 1.0, "Nist model returned probability > 1");
        
        NS_TEST_ASSERT_MSG_GE(psTable, 0.0, "Table model returned probability < 0");
        NS_TEST_ASSERT_MSG_LE(psTable, 1.0, "Table model returned probability > 1");
    }
};

static ErrorRateModelTest errorRateModelTestInstance;
