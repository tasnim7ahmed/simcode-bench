#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t frameSize = 1000;
    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.Parse(argc, argv);

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Frame Success Rate vs. SNR");
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    std::vector<std::string> dsssModes = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};

    for (const auto& mode : dsssModes) {
        phyMode = mode;
        std::vector<double> snrValues;
        std::vector<double> successRatesYans;
        std::vector<double> successRatesNist;
        std::vector<double> successRatesTable;

        for (double snr = 0.0; snr <= 20.0; snr += 1.0) {
            snrValues.push_back(snr);

            // Yans Error Rate Model
            YansErrorRateModel yansErrorRateModel;
            double berYans = yansErrorRateModel.GetBitErrorRate(snr, phyMode);
            double ferYans = 1 - pow((1 - berYans), (8 * frameSize));
            successRatesYans.push_back(1 - ferYans);

            // Nist Error Rate Model
            NistErrorRateModel nistErrorRateModel;
            double berNist = nistErrorRateModel.GetBitErrorRate(snr, phyMode);
            double ferNist = 1 - pow((1 - berNist), (8 * frameSize));
            successRatesNist.push_back(1 - ferNist);

            // Table Error Rate Model (Using a dummy table for demonstration)
            TableErrorRateModel tableErrorRateModel;
            tableErrorRateModel.SetDefaultBer(berYans);  // Use Yans BER for the table model. Can be replaced by an actual table.
            double berTable = tableErrorRateModel.GetBitErrorRate(snr, phyMode);
            double ferTable = 1 - pow((1 - berTable), (8 * frameSize));
            successRatesTable.push_back(1 - ferTable);
        }

        std::string graphTitleYans = mode + " (Yans)";
        for (size_t i = 0; i < snrValues.size(); ++i) {
            dataset.Add(snrValues[i], successRatesYans[i]);
        }
        dataset.SetTitle(graphTitleYans);

        std::string filenamePrefix = "dsss-error-rate-" + mode;
        std::string plotFilename = filenamePrefix + ".plt";
        std::string imageFilename = filenamePrefix + ".eps";

        Gnuplot plot;
        plot.SetTitle("Frame Success Rate vs. SNR for " + mode);
        plot.SetLegend("SNR (dB)", "Frame Success Rate");
        plot.AddDataset(dataset);
        plot.SetTerminal("postscript eps color");
        plot.SetOutputFileName(imageFilename);
        plot.GeneratePlotFile(plotFilename);

        dataset.Clear();

        std::string graphTitleNist = mode + " (Nist)";
        for (size_t i = 0; i < snrValues.size(); ++i) {
            dataset.Add(snrValues[i], successRatesNist[i]);
        }
        dataset.SetTitle(graphTitleNist);

        filenamePrefix = "dsss-error-rate-" + mode + "-nist";
        plotFilename = filenamePrefix + ".plt";
        imageFilename = filenamePrefix + ".eps";

        plot.ClearDatasets();
        plot.SetTitle("Frame Success Rate vs. SNR for " + mode + " (Nist)");
        plot.AddDataset(dataset);
        plot.SetOutputFileName(imageFilename);
        plot.GeneratePlotFile(plotFilename);

        dataset.Clear();

       std::string graphTitleTable = mode + " (Table)";
        for (size_t i = 0; i < snrValues.size(); ++i) {
            dataset.Add(snrValues[i], successRatesTable[i]);
        }
        dataset.SetTitle(graphTitleTable);

        filenamePrefix = "dsss-error-rate-" + mode + "-table";
        plotFilename = filenamePrefix + ".plt";
        imageFilename = filenamePrefix + ".eps";

        plot.ClearDatasets();
        plot.SetTitle("Frame Success Rate vs. SNR for " + mode + " (Table)");
        plot.AddDataset(dataset);
        plot.SetOutputFileName(imageFilename);
        plot.GeneratePlotFile(plotFilename);

        dataset.Clear();
    }

    return 0;
}