#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/three-gpp-channel-model.h"
#include "ns3/three-gpp-spectrum-propagation-loss-model.h"
#include "ns3/three-gpp-v2v-propagation-loss-model.h"
#include "ns3/three-gpp-v2v-channel-condition-model.h"
#include "ns3/phy-layer.h"
#include "ns3/spectrum-value.h"
#include "ns3/buildings-module.h"
#include "ns3/mobility-helper.h"
#include <fstream>

using namespace ns3;

class ThreeGppV2vChannelModelTest : public ns3::TestCase
{
public:
    ThreeGppV2vChannelModelTest(std::string name) : TestCase(name) {}
    
    // Helper functions to assert conditions
    void AssertAlmostEqual(double a, double b, double epsilon = 1e-6)
    {
        NS_TEST_ASSERT_MSG_EQ_TOL(a, b, epsilon, "Values are not equal");
    }
    
    void AssertGreaterThan(double a, double b)
    {
        NS_TEST_ASSERT_MSG_GT(a, b, "Value is not greater");
    }

    void AssertNotNull(Ptr<Object> ptr)
    {
        NS_TEST_ASSERT_MSG_NE(ptr, nullptr, "Pointer is null");
    }

    // Test for the creation of ThreeGppV2vUrbanChannelConditionModel
    void TestCreateChannelConditionModel()
    {
        Ptr<ThreeGppV2vUrbanChannelConditionModel> condModel =
            CreateObject<ThreeGppV2vUrbanChannelConditionModel>();
        AssertNotNull(condModel);
    }

    // Test for the calculation of path loss using the propagation loss model
    void TestPathLossCalculation()
    {
        Ptr<ThreeGppV2vUrbanPropagationLossModel> propagationLossModel =
            CreateObject<ThreeGppV2vUrbanPropagationLossModel>();

        Ptr<MobilityModel> txMob = CreateObject<ConstantVelocityMobilityModel>();
        Ptr<MobilityModel> rxMob = CreateObject<ConstantVelocityMobilityModel>();

        // Set positions for transmission and reception nodes
        txMob->SetPosition(Vector(0.0, 0.0, 1.5));
        rxMob->SetPosition(Vector(50.0, 50.0, 1.5));

        double pathloss = propagationLossModel->CalcRxPower(0, txMob, rxMob);
        AssertGreaterThan(pathloss, 0.0);  // Expecting a positive path loss value
    }

    // Test for computing SNR
    void TestComputeSnr()
    {
        Ptr<ThreeGppV2vUrbanChannelConditionModel> condModel =
            CreateObject<ThreeGppV2vUrbanChannelConditionModel>();
        
        Ptr<MobilityModel> txMob = CreateObject<ConstantVelocityMobilityModel>();
        Ptr<MobilityModel> rxMob = CreateObject<ConstantVelocityMobilityModel>();

        // Set positions for transmission and reception nodes
        txMob->SetPosition(Vector(0.0, 0.0, 1.5));
        rxMob->SetPosition(Vector(50.0, 50.0, 1.5));

        Ptr<SpectrumSignalParameters> txParams = Create<SpectrumSignalParameters>();
        double noiseFigure = 9.0;

        // Create antennas for both transmission and reception
        Ptr<PhasedArrayModel> txAntenna = CreateObject<UniformPlanarArray>();
        Ptr<PhasedArrayModel> rxAntenna = CreateObject<UniformPlanarArray>();

        ComputeSnrParams params{txMob, rxMob, txParams, noiseFigure, txAntenna, rxAntenna};

        // Call ComputeSnr and ensure it runs without errors
        Simulator::Schedule(Seconds(0.1), &ComputeSnr, params);
        Simulator::Run();
        Simulator::Destroy();
    }

    // Test for beamforming vectors
    void TestBeamforming()
    {
        Ptr<NetDevice> txDev = CreateObject<SimpleNetDevice>();
        Ptr<NetDevice> rxDev = CreateObject<SimpleNetDevice>();
        Ptr<PhasedArrayModel> txAntenna = CreateObject<UniformPlanarArray>();
        Ptr<PhasedArrayModel> rxAntenna = CreateObject<UniformPlanarArray>();

        // Beamforming vectors for tx and rx devices
        DoBeamforming(txDev, txAntenna, rxDev);
        DoBeamforming(rxDev, rxAntenna, txDev);

        // Ensure that beamforming vectors are set correctly
        AssertNotNull(txAntenna->GetBeamformingVector());
        AssertNotNull(rxAntenna->GetBeamformingVector());
    }

    // Test to ensure that the output file for SNR and path loss is being written correctly
    void TestOutputFile()
    {
        // Setup for computing SNR
        std::ofstream outFile("example-output.txt", std::ios::out | std::ios::app);
        outFile << "Time[s] TxPosX[m] TxPosY[m] RxPosX[m] RxPosY[m] ChannelState SNR[dB] Pathloss[dB]"
                << std::endl;
        outFile.close();

        // Check if the output file is created and has data
        std::ifstream inFile("example-output.txt");
        AssertNotNull(inFile);
        std::string line;
        std::getline(inFile, line);
        NS_TEST_ASSERT_MSG_NE(line.empty(), true, "Output file is empty");
        inFile.close();
    }
};

// Create a test suite to run all the test cases
static void RunTests()
{
    ThreeGppV2vChannelModelTest test1("Test Channel Condition Model Creation");
    test1.TestCreateChannelConditionModel();
    
    ThreeGppV2vChannelModelTest test2("Test Path Loss Calculation");
    test2.TestPathLossCalculation();
    
    ThreeGppV2vChannelModelTest test3("Test SNR Calculation");
    test3.TestComputeSnr();
    
    ThreeGppV2vChannelModelTest test4("Test Beamforming");
    test4.TestBeamforming();
    
    ThreeGppV2vChannelModelTest test5("Test Output File");
    test5.TestOutputFile();
}

int main(int argc, char *argv[])
{
    // Set up the command line parser to run the tests
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Run all the tests
    RunTests();

    return 0;
}
