#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRateValidation");

int main(int argc, char *argv[]) {
    bool enableNist = true;
    bool enableYans = true;
    bool enableTableBased = true;
    uint32_t frameSize = 1000;

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("enableNist", "Enable NIST error model", enableNist);
    cmd.AddValue("enableYans", "Enable YANS error model", enableYans);
    cmd.AddValue("enableTableBased", "Enable Table Based error model", enableTableBased);
    cmd.Parse(argc, argv);

    LogComponentEnable("VhtErrorRateValidation", LOG_LEVEL_INFO);

    double snrStart = -5.0;
    double snrStop = 30.0;
    double snrStep = 1.0;

    std::vector<std::string> modes = {"VhtMcs0", "VhtMcs1", "VhtMcs2", "VhtMcs3", "VhtMcs4", "VhtMcs5", "VhtMcs6", "VhtMcs7", "VhtMcs8"};

    for (std::string mode : modes) {
        Gnuplot nistPlot;
        Gnuplot yansPlot;
        Gnuplot tableBasedPlot;

        nistPlot.SetTitle("NIST Error Rate Model - " + mode);
        yansPlot.SetTitle("YANS Error Rate Model - " + mode);
        tableBasedPlot.SetTitle("Table Based Error Rate Model - " + mode);

        nistPlot.SetLegend("SNR (dB)", "Frame Success Rate");
        yansPlot.SetLegend("SNR (dB)", "Frame Success Rate");
        tableBasedPlot.SetLegend("SNR (dB)", "Frame Success Rate");

        std::vector<std::pair<double, double>> nistData;
        std::vector<std::pair<double, double>> yansData;
        std::vector<std::pair<double, double>> tableBasedData;

        for (double snr = snrStart; snr <= snrStop; snr += snrStep) {
            double berNist = 0.0;
            double berYans = 0.0;
            double berTableBased = 0.0;

            if(enableNist){
              NistErrorRateModel nistErrorRateModel;
              berNist = nistErrorRateModel.GetBerFromSnr(snr, mode, 20);
            }
            if(enableYans){
              YansErrorRateModel yansErrorRateModel;
              berYans = yansErrorRateModel.GetBerFromSnr(snr, mode);
            }
            if(enableTableBased){
              TableBasedErrorRateModel tableBasedErrorRateModel;
              berTableBased = tableBasedErrorRateModel.GetBerFromSnr(snr, mode, 20);
            }

            double ferNist = 1.0 - std::pow((1.0 - berNist), (frameSize * 8.0));
            double ferYans = 1.0 - std::pow((1.0 - berYans), (frameSize * 8.0));
            double ferTableBased = 1.0 - std::pow((1.0 - berTableBased), (frameSize * 8.0));

            double successRateNist = 1.0 - ferNist;
            double successRateYans = 1.0 - ferYans;
            double successRateTableBased = 1.0 - ferTableBased;

            if(enableNist) nistData.push_back(std::make_pair(snr, successRateNist));
            if(enableYans) yansData.push_back(std::make_pair(snr, successRateYans));
            if(enableTableBased) tableBasedData.push_back(std::make_pair(snr, successRateTableBased));

            NS_LOG_INFO("SNR: " << snr << " dB, Mode: " << mode
                        << (enableNist ? ", NIST Success Rate: " + std::to_string(successRateNist) : "")
                        << (enableYans ? ", YANS Success Rate: " + std::to_string(successRateYans) : "")
                        << (enableTableBased ? ", Table Based Success Rate: " + std::to_string(successRateTableBased) : ""));
        }
        if(enableNist){
          nistPlot.AddDataset(mode, nistData);
          nistPlot.GenerateOutput("nist_" + mode + ".plt");
        }
        if(enableYans){
          yansPlot.AddDataset(mode, yansData);
          yansPlot.GenerateOutput("yans_" + mode + ".plt");
        }
        if(enableTableBased){
          tableBasedPlot.AddDataset(mode, tableBasedData);
          tableBasedPlot.GenerateOutput("table_" + mode + ".plt");
        }
    }

    return 0;
}