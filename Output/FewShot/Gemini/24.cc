#include "ns3/core-module.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/command-line.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-phy.h"
#include "ns3/gnuplot.h"
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableNist = true;
    bool enableYans = true;
    bool enableTable = true;
    uint32_t frameSize = 1000;
    uint32_t mcsStart = 0;
    uint32_t mcsEnd = 7;
    uint32_t step = 1;

    CommandLine cmd;
    cmd.AddValue("enableNist", "Enable NIST error model", enableNist);
    cmd.AddValue("enableYans", "Enable YANS error model", enableYans);
    cmd.AddValue("enableTable", "Enable Table error model", enableTable);
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("mcsStart", "Starting MCS value", mcsStart);
    cmd.AddValue("mcsEnd", "Ending MCS value", mcsEnd);
    cmd.AddValue("step", "MCS increment step", step);
    cmd.Parse(argc, argv);

    std::vector<double> snrValues = {5, 10, 15, 20, 25, 30};

    Gnuplot2dDataset datasetNist;
    Gnuplot2dDataset datasetYans;
    Gnuplot2dDataset datasetTable;

    datasetNist.SetStyle(Gnuplot2dDataset::LINES);
    datasetYans.SetStyle(Gnuplot2dDataset::LINES);
    datasetTable.SetStyle(Gnuplot2dDataset::LINES);

    datasetNist.SetTitle("NIST");
    datasetYans.SetTitle("YANS");
    datasetTable.SetTitle("Table");

    Gnuplot plot("fer_vs_snr.eps");
    plot.SetTitle("FER vs SNR");
    plot.SetLegend("SNR (dB)", "FER");

    for (uint32_t mcs = mcsStart; mcs <= mcsEnd; mcs += step) {
        std::vector<std::pair<double, double>> nistData, yansData, tableData;

        for (double snr : snrValues) {
            double ferNist = 0.0, ferYans = 0.0, ferTable = 0.0;
            uint32_t numFrames = 1000;
            uint32_t failedFramesNist = 0, failedFramesYans = 0, failedFramesTable = 0;

            for (uint32_t i = 0; i < numFrames; ++i) {
                if (enableNist) {
                    Ptr<ErrorModel> emNist = CreateObject<NistErrorRateModel>();
                    emNist->SetAttribute("FrameSize", UintegerValue(frameSize));
                    emNist->SetAttribute("Mcs", UintegerValue(mcs));
                    double berNist = DynamicCast<NistErrorRateModel>(emNist)->GetBerFromSnr(snr);
                    double perNist = 1 - pow((1 - berNist), (8 * frameSize));
                    if (UniformRandomVariable().GetValue() < perNist) {
                        failedFramesNist++;
                    }
                }

                if (enableYans) {
                    Ptr<ErrorModel> emYans = CreateObject<YansErrorRateModel>();
                    emYans->SetAttribute("FrameSize", UintegerValue(frameSize));
                    emYans->SetAttribute("Mcs", UintegerValue(mcs));
                    double berYans = DynamicCast<YansErrorRateModel>(emYans)->GetBerFromSnr(snr);
                    double perYans = 1 - pow((1 - berYans), (8 * frameSize));
                    if (UniformRandomVariable().GetValue() < perYans) {
                        failedFramesYans++;
                    }
                }

                if (enableTable) {
                    Ptr<ErrorModel> emTable = CreateObject<RateErrorModel>();
                    emTable->SetAttribute("ErrorRate", DoubleValue(0.0));
                    emTable->SetAttribute("RandomVariable", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));

                    // Placeholder for actual Table Error Model implementation
                    // This assumes a fixed FER of 0.1 for demonstration
                    double perTable = 0.1;
                    if (UniformRandomVariable().GetValue() < perTable) {
                        failedFramesTable++;
                    }
                }
            }

            if (enableNist) {
                ferNist = (double)failedFramesNist / numFrames;
                nistData.push_back(std::make_pair(snr, ferNist));
            }
            if (enableYans) {
                ferYans = (double)failedFramesYans / numFrames;
                yansData.push_back(std::make_pair(snr, ferYans));
            }
            if (enableTable) {
                ferTable = (double)failedFramesTable / numFrames;
                tableData.push_back(std::make_pair(snr, ferTable));
            }
        }
        if (enableNist){
            datasetNist.Add(nistData);
            datasetNist.SetTitle("NIST MCS=" + std::to_string(mcs));
            plot.AddDataset(datasetNist);
            datasetNist.Clear();
        }

        if (enableYans){
            datasetYans.Add(yansData);
            datasetYans.SetTitle("YANS MCS=" + std::to_string(mcs));
            plot.AddDataset(datasetYans);
            datasetYans.Clear();
        }

        if (enableTable){
            datasetTable.Add(tableData);
            datasetTable.SetTitle("Table MCS=" + std::to_string(mcs));
            plot.AddDataset(datasetTable);
            datasetTable.Clear();
        }
    }

    plot.GenerateOutput();

    return 0;
}