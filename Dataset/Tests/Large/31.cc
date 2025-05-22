#include "ns3/test.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/gnuplot.h"

using namespace ns3;

// Test Case: Verify error rate models are instantiated correctly
class ErrorRateModelCreationTest : public TestCase
{
public:
    ErrorRateModelCreationTest() : TestCase("Error Rate Model Creation Test") {}

    virtual void DoRun()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();

        NS_TEST_ASSERT_MSG_NE(yans, nullptr, "Failed to create YansErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(nist, nullptr, "Failed to create NistErrorRateModel");
        NS_TEST_ASSERT_MSG_NE(table, nullptr, "Failed to create TableBasedErrorRateModel");
    }
};

// Test Case: Verify valid SNR conversion
class SNRCalculationTest : public TestCase
{
public:
    SNRCalculationTest() : TestCase("SNR Calculation Test") {}

    virtual void DoRun()
    {
        double snrDb = 10.0;
        double snr = std::pow(10.0, snrDb / 10.0);
        NS_TEST_ASSERT_MSG_GT(snr, 0.0, "SNR should be positive");
    }
};

// Test Case: Validate chunk success rate calculations
class ChunkSuccessRateTest : public TestCase
{
public:
    ChunkSuccessRateTest() : TestCase("Chunk Success Rate Test") {}

    virtual void DoRun()
    {
        Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
        Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
        Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
        WifiTxVector txVector;
        txVector.SetMode("EhtMcs5");
        WifiMode wifiMode("EhtMcs5");

        uint32_t frameSizeBits = 1500 * 8;
        double snr = std::pow(10.0, 10.0 / 10.0);

        double yansPs = yans->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
        double nistPs = nist->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);
        double tablePs = table->GetChunkSuccessRate(wifiMode, txVector, snr, frameSizeBits);

        NS_TEST_ASSERT_MSG_GE(yansPs, 0.0, "Yans success rate should be >= 0");
        NS_TEST_ASSERT_MSG_LE(yansPs, 1.0, "Yans success rate should be <= 1");
        NS_TEST_ASSERT_MSG_GE(nistPs, 0.0, "Nist success rate should be >= 0");
        NS_TEST_ASSERT_MSG_LE(nistPs, 1.0, "Nist success rate should be <= 1");
        NS_TEST_ASSERT_MSG_GE(tablePs, 0.0, "Table success rate should be >= 0");
        NS_TEST_ASSERT_MSG_LE(tablePs, 1.0, "Table success rate should be <= 1");
    }
};

// Test Case: Verify Gnuplot output files are generated
class GnuplotGenerationTest : public TestCase
{
public:
    GnuplotGenerationTest() : TestCase("Gnuplot Output Test") {}

    virtual void DoRun()
    {
        std::ofstream testfile("test-output.plt");
        Gnuplot testPlot = Gnuplot("test-output.eps");
        testPlot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
        testPlot.SetLegend("SNR(dB)", "Frame Success Rate");
        testPlot.GenerateOutput(testfile);
        testfile.close();

        std::ifstream infile("test-output.plt");
        NS_TEST_ASSERT_MSG_EQ(infile.good(), true, "Failed to generate Gnuplot file");
    }
};

// Define the test suite
class Ns3UnitTestSuite : public TestSuite
{
public:
    Ns3UnitTestSuite() : TestSuite("ns3-unit-tests", UNIT)
    {
        AddTestCase(new ErrorRateModelCreationTest, TestCase::QUICK);
        AddTestCase(new SNRCalculationTest, TestCase::QUICK);
        AddTestCase(new ChunkSuccessRateTest, TestCase::QUICK);
        AddTestCase(new GnuplotGenerationTest, TestCase::EXTENSIVE);
    }
};

// Register the test suite
static Ns3UnitTestSuite ns3UnitTestSuite;
