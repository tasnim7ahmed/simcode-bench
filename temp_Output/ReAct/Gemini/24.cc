#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRateModelsComparison");

int main(int argc, char *argv[]) {
    bool enableNist = true;
    bool enableYans = true;
    bool enableTable = true;
    uint32_t frameSize = 1000;
    uint32_t mcsStart = 0;
    uint32_t mcsEnd = 7;
    uint32_t mcsStep = 4;
    double snrStart = 5.0;
    double snrEnd = 30.0;
    double snrStep = 5.0;
    std::string phyMode("OfdmRate6Mbps");

    CommandLine cmd;
    cmd.AddValue("enableNist", "Enable Nist error model", enableNist);
    cmd.AddValue("enableYans", "Enable Yans error model", enableYans);
    cmd.AddValue("enableTable", "Enable Table-based error model", enableTable);
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("mcsStart", "Starting MCS value", mcsStart);
    cmd.AddValue("mcsEnd", "Ending MCS value", mcsEnd);
    cmd.AddValue("mcsStep", "MCS step value", mcsStep);
    cmd.AddValue("snrStart", "Starting SNR value", snrStart);
    cmd.AddValue("snrEnd", "Ending SNR value", snrEnd);
    cmd.AddValue("snrStep", "SNR step value", snrStep);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.Parse(argc, argv);

    LogComponentEnable("ErrorRateModelsComparison", LOG_LEVEL_INFO);

    Gnuplot2dDataset datasetNist;
    Gnuplot2dDataset datasetYans;
    Gnuplot2dDataset datasetTable;

    datasetNist.SetStyle(Gnuplot2dDataset::LINES);
    datasetYans.SetStyle(Gnuplot2dDataset::LINES);
    datasetTable.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot plot("error_rate_models.eps");
    plot.SetTitle("Frame Error Rate vs SNR");
    plot.SetXTitle("SNR (dB)");
    plot.SetYTitle("FER");

    for (uint32_t mcs = mcsStart; mcs <= mcsEnd; mcs += mcsStep) {
        std::stringstream ss;
        ss << "VhtMcs" << mcs;
        phyMode = ss.str();

        std::vector<std::pair<double, double>> nistData;
        std::vector<std::pair<double, double>> yansData;
        std::vector<std::pair<double, double>> tableData;

        for (double snr = snrStart; snr <= snrEnd; snr += snrStep) {
            double ferNist = 0.0;
            double ferYans = 0.0;
            double ferTable = 0.0;

            if (enableNist) {
                NistErrorRateModel errorModelNist;
                errorModelNist.SetPhyMode(phyMode);
                errorModelNist.SetSinr(snr);
                ferNist = errorModelNist.CalculateErrorRate(frameSize * 8);
                nistData.push_back(std::make_pair(snr, ferNist));
            }

            if (enableYans) {
                YansErrorRateModel errorModelYans;
                errorModelYans.SetPhyMode(phyMode);
                errorModelYans.SetSinr(snr);
                ferYans = errorModelYans.CalculateErrorRate(frameSize * 8);
                yansData.push_back(std::make_pair(snr, ferYans));
            }

            if (enableTable) {
                TableBasedErrorRateModel errorModelTable;
                errorModelTable.SetPhyMode(phyMode);
                errorModelTable.SetSinr(snr);
                ferTable = errorModelTable.CalculateErrorRate(frameSize * 8);
                tableData.push_back(std::make_pair(snr, ferTable));
            }
        }

        if (enableNist) {
            std::stringstream labelNist;
            labelNist << "Nist (MCS " << mcs << ")";
            datasetNist.AddMany(nistData);
            plot.AddDataset(datasetNist, labelNist.str());
            datasetNist.Clear();
        }

        if (enableYans) {
            std::stringstream labelYans;
            labelYans << "Yans (MCS " << mcs << ")";
            datasetYans.AddMany(yansData);
            plot.AddDataset(datasetYans, labelYans.str());
            datasetYans.Clear();
        }

        if (enableTable) {
            std::stringstream labelTable;
            labelTable << "Table (MCS " << mcs << ")";
            datasetTable.AddMany(tableData);
            plot.AddDataset(datasetTable, labelTable.str());
            datasetTable.Clear();
        }
    }

    plot.GenerateOutput();

    return 0;
}