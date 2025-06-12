#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/error-rate-model.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {
    // Configure simulation parameters
    uint32_t frameSize = 1000;
    double minSnr = 0.0;
    double maxSnr = 25.0;
    uint32_t snrSteps = 100;

    // Configure Gnuplot
    std::string fileNameWithNoExtension = "dsss-error-models";
    std::string graphicsFileName        = fileNameWithNoExtension + ".eps";
    std::string plotFileName            = fileNameWithNoExtension + ".plt";
    std::string dataFileName            = fileNameWithNoExtension + ".dat";

    Gnuplot plot(graphicsFileName);
    plot.SetTitle("Frame Success Rate vs. SNR");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");

    // DSSS modes
    std::vector<WifiMode> dsssModes = {
        WifiMode("DsssRate1Mbps"),
        WifiMode("DsssRate2Mbps"),
        WifiMode("DsssRate5_5Mbps"),
        WifiMode("DsssRate11Mbps")
    };

    // Error Rate Models
    YansErrorRateModel yansErrorRateModel;
    NistErrorRateModel nistErrorRateModel;
    TableBasedErrorRateModel tableErrorRateModel; // Assuming a default table is loaded externally.

    // Create Gnuplot dataset
    Gnuplot2dDataset yansDataset;
    yansDataset.SetTitle("Yans Error Model");
    yansDataset.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot2dDataset nistDataset;
    nistDataset.SetTitle("NIST Error Model");
    nistDataset.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot2dDataset tableDataset;
    tableDataset.SetTitle("Table Error Model");
    tableDataset.SetStyle(Gnuplot2dDataset::LINES);


    for (const auto& mode : dsssModes) {
        std::cout << "Processing mode: " << mode << std::endl;

        for (uint32_t i = 0; i <= snrSteps; ++i) {
            double snr = minSnr + (maxSnr - minSnr) * static_cast<double>(i) / snrSteps;

            // Calculate Frame Success Rate for each model
            double yansSuccessRate = 1.0 - yansErrorRateModel.CalculateErrorRate(snr, mode, frameSize);
            double nistSuccessRate = 1.0 - nistErrorRateModel.CalculateErrorRate(snr, mode, frameSize);
            double tableSuccessRate = 1.0 - tableErrorRateModel.CalculateErrorRate(snr, mode, frameSize);

            // Validation (replace with appropriate checks)
            if (yansSuccessRate < 0.0 || yansSuccessRate > 1.0) {
                std::cerr << "Yans Success Rate out of range: " << yansSuccessRate << std::endl;
            }
            if (nistSuccessRate < 0.0 || nistSuccessRate > 1.0) {
                std::cerr << "NIST Success Rate out of range: " << nistSuccessRate << std::endl;
            }
             if (tableSuccessRate < 0.0 || tableSuccessRate > 1.0) {
                std::cerr << "Table Success Rate out of range: " << tableSuccessRate << std::endl;
            }

            // Store data for Gnuplot
            yansDataset.Add(snr, yansSuccessRate);
            nistDataset.Add(snr, nistSuccessRate);
            tableDataset.Add(snr, tableSuccessRate);
        }

        plot.AddDataset(yansDataset);
        plot.AddDataset(nistDataset);
        plot.AddDataset(tableDataset);


        std::ofstream dataFile(dataFileName.c_str());
        plot.GenerateOutput(dataFile);
        dataFile.close();
    }

    return 0;
}