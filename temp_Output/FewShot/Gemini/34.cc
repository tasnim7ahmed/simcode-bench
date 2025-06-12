#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/gnuplot.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
    // Command line arguments
    uint32_t frameSize = 1000; // bytes
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.Parse(argc, argv);

    // Configure simulation parameters
    double minSnr = 0.0;
    double maxSnr = 30.0;
    double snrStep = 1.0;

    // Define HE MCS values
    std::vector<uint8_t> mcsValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    // Error Rate Model Names
    std::vector<std::string> errorRateModelNames = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};

    // Output files
    std::map<std::string, std::map<uint8_t, std::string>> dataFileNames;
    std::map<std::string, std::map<uint8_t, std::ofstream>> dataFiles;

    for (const auto& modelName : errorRateModelNames) {
        for (const auto& mcs : mcsValues) {
            dataFileNames[modelName][mcs] = modelName + "-MCS" + std::to_string(mcs) + ".dat";
        }
    }

    // Open data files
    for (const auto& modelName : errorRateModelNames) {
        for (const auto& mcs : mcsValues) {
            dataFiles[modelName][mcs].open(dataFileNames[modelName][mcs].c_str());
            dataFiles[modelName][mcs] << "# SNR FrameSuccessRate" << std::endl;
        }
    }

    // Simulation loop
    for (double snr = minSnr; snr <= maxSnr; snr += snrStep) {
        if(verbose){
            std::cout << "SNR: " << snr << std::endl;
        }

        for (const auto& modelName : errorRateModelNames) {
            for (const auto& mcs : mcsValues) {
                // Configure error rate model
                Config::SetDefault("ns3::WifiNetDevice::Phy/ErrorRateModel", StringValue(modelName));
                if (modelName == "TableBasedErrorRateModel") {
                    Config::SetDefault("ns3::TableBasedErrorRateModel::FileName", StringValue("wifi_he_mcs.txt"));
                }

                // Create an error model object
                Ptr<ErrorRateModel> errorRateModel = DynamicCast<ErrorRateModel>(GetObject<ErrorRateModel>());

                // Calculate frame success rate
                double successRate = 1.0 - errorRateModel->GetErrorRate(snr, WifiMode::GetHeMcs(mcs), frameSize);

                // Write to data file
                dataFiles[modelName][mcs] << snr << " " << successRate << std::endl;

                if(verbose){
                    std::cout << "  " << modelName << " MCS " << (int)mcs << ": " << successRate << std::endl;
                }
            }
        }
    }

    // Close data files
    for (const auto& modelName : errorRateModelNames) {
        for (const auto& mcs : mcsValues) {
            dataFiles[modelName][mcs].close();
        }
    }

    // Generate Gnuplot script
    Gnuplot gnuplot;
    gnuplot.SetTitle("Frame Success Rate vs. SNR");
    gnuplot.SetTerminal("png");
    gnuplot.SetOutput("frame-success-rate.png");
    gnuplot.SetXRange(minSnr, maxSnr);
    gnuplot.SetYRange(0.0, 1.0);
    gnuplot.Setxlabel("SNR (dB)");
    gnuplot.Setylabel("Frame Success Rate");

    Gnuplot2dDataset dataset;
    for (const auto& modelName : errorRateModelNames) {
        for (const auto& mcs : mcsValues) {
            dataset = Gnuplot2dDataset(dataFileNames[modelName][mcs]);
            dataset.SetTitle(modelName + " MCS " + std::to_string(mcs));
            dataset.SetStyle(modelName == "NistErrorRateModel" ? "lines" : (modelName == "YansErrorRateModel" ? "lines" : "lines"));
            gnuplot.AddDataset(dataset);
        }
    }

    gnuplot.GenerateOutput(std::cout);

    return 0;
}