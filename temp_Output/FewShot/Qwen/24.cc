#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorModelComparison");

struct SimulationParams {
    uint32_t frameSize;
    uint8_t mcsMin;
    uint8_t mcsMax;
    uint8_t mcsStep;
    double snrMin;
    double snrMax;
    double snrStep;
    bool useNist;
    bool useYans;
    bool useTable;
};

void CalculateAndPlotFER(const SimulationParams& params) {
    Gnuplot plot = Gnuplot("fer_comparison.eps");
    plot.SetTerminal("postscript eps color enhanced");
    plot.SetTitle("Frame Error Rate vs SNR for Different Error Models");
    plot.SetLegend("SNR (dB)", "FER");
    plot.SetExtra("set logscale y");

    if (params.useNist) {
        Ptr<YansWifiPhy> phyNist = CreateObject<YansWifiPhy>();
        phyNist->SetErrorRateModel(CreateObject<NistErrorRateModel>());
        Gnuplot2dDataset datasetNist("Nist Model");
        datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        datasetNist.SetColor("blue");

        for (double snr = params.snrMin; snr <= params.snrMax; snr += params.snrStep) {
            for (uint8_t mcs = params.mcsMin; mcs <= params.mcsMax; mcs += params.mcsStep) {
                WifiMode mode = WifiPhy::GetHtMcs(mcs);
                double fer = phyNist->GetErrorRateModel()->GetFer(phyNist, mode, params.frameSize, Seconds(0.1), DbmToW(-snr), 0);
                datasetNist.Add(snr, fer);
            }
        }
        plot.AddDataset(datasetNist);
    }

    if (params.useYans) {
        Ptr<YansWifiPhy> phyYans = CreateObject<YansWifiPhy>();
        phyYans->SetErrorRateModel(CreateObject<YansErrorRateModel>());
        Gnuplot2dDataset datasetYans("Yans Model");
        datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        datasetYans.SetColor("red");

        for (double snr = params.snrMin; snr <= params.snrMax; snr += params.snrStep) {
            for (uint8_t mcs = params.mcsMin; mcs <= params.mcsMax; mcs += params.mcsStep) {
                WifiMode mode = WifiPhy::GetHtMcs(mcs);
                double fer = phyYans->GetErrorRateModel()->GetFer(phyYans, mode, params.frameSize, Seconds(0.1), DbmToW(-snr), 0);
                datasetYans.Add(snr, fer);
            }
        }
        plot.AddDataset(datasetYans);
    }

    if (params.useTable) {
        Ptr<YansWifiPhy> phyTable = CreateObject<YansWifiPhy>();
        phyTable->SetErrorRateModel(CreateObject<TableBasedErrorRateModel>());
        Gnuplot2dDataset datasetTable("Table-based Model");
        datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        datasetTable.SetColor("green");

        for (double snr = params.snrMin; snr <= params.snrMax; snr += params.snrStep) {
            for (uint8_t mcs = params.mcsMin; mcs <= params.mcsMax; mcs += params.mcsStep) {
                WifiMode mode = WifiPhy::GetHtMcs(mcs);
                double fer = phyTable->GetErrorRateModel()->GetFer(phyTable, mode, params.frameSize, Seconds(0.1), DbmToW(-snr), 0);
                datasetTable.Add(snr, fer);
            }
        }
        plot.AddDataset(datasetTable);
    }

    std::ofstream ofs("fer_comparison.plt");
    plot.GenerateOutput(ofs);
}

int main(int argc, char *argv[]) {
    SimulationParams params;
    params.frameSize = 1500;
    params.mcsMin = 0;
    params.mcsMax = 7;
    params.mcsStep = 1;
    params.snrMin = 0.0;
    params.snrMax = 30.0;
    params.snrStep = 2.0;
    params.useNist = true;
    params.useYans = true;
    params.useTable = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "Frame size in bytes", params.frameSize);
    cmd.AddValue("mcsMin", "Minimum MCS value to test", params.mcsMin);
    cmd.AddValue("mcsMax", "Maximum MCS value to test", params.mcsMax);
    cmd.AddValue("mcsStep", "Step between MCS values", params.mcsStep);
    cmd.AddValue("snrMin", "Minimum SNR (dB) to simulate", params.snrMin);
    cmd.AddValue("snrMax", "Maximum SNR (dB) to simulate", params.snrMax);
    cmd.AddValue("snrStep", "Step between SNR values", params.snrStep);
    cmd.AddValue("useNist", "Enable NIST error model", params.useNist);
    cmd.AddValue("useYans", "Enable YANS error model", params.useYans);
    cmd.AddValue("useTable", "Enable Table-based error model", params.useTable);

    cmd.Parse(argc, argv);

    CalculateAndPlotFER(params);

    return 0;
}