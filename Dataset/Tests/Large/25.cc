#include "ns3/test.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

class CommandLineTestCase : public TestCase {
public:
    CommandLineTestCase() : TestCase("Command Line Parsing Test") {}
    virtual void DoRun() {
        CommandLine cmd;
        std::string phyModel = "YansWifiPhy";
        cmd.AddValue("phyModel", "PHY model type", phyModel);
        char* argv[] = {(char*)"program", (char*)"--phyModel=NistWifiPhy"};
        int argc = 2;
        cmd.Parse(argc, argv);
        NS_TEST_ASSERT_MSG_EQ(phyModel, "NistWifiPhy", "Command line parsing failed");
    }
};

class ConfigSettingTestCase : public TestCase {
public:
    ConfigSettingTestCase() : TestCase("Config Setting Test") {}
    virtual void DoRun() {
        Config::SetDefault("ns3::YansWifiPhy::TxGain", DoubleValue(5.0));
        double txGain;
        Config::GetDefault("ns3::YansWifiPhy::TxGain", txGain);
        NS_TEST_ASSERT_MSG_EQ(txGain, 5.0, "TxGain setting failed");
    }
};

class ThroughputCalculationTestCase : public TestCase {
public:
    ThroughputCalculationTestCase() : TestCase("Throughput Calculation Test") {}
    virtual void DoRun() {
        uint64_t totalBytesReceived = 1000000;
        double simulationTime = 5.0; // seconds
        double throughput = (totalBytesReceived * 8) / (simulationTime * 1e6); // Mbps
        NS_TEST_ASSERT_MSG_EQ_TOL(throughput, 1.6, 0.1, "Throughput calculation incorrect");
    }
};

class MyTestSuite : public TestSuite {
public:
    MyTestSuite() : TestSuite("ns3-unit-tests", UNIT) {
        AddTestCase(new CommandLineTestCase(), TestCase::QUICK);
        AddTestCase(new ConfigSettingTestCase(), TestCase::QUICK);
        AddTestCase(new ThroughputCalculationTestCase(), TestCase::QUICK);
    }
};

static MyTestSuite g_myTestSuite;
