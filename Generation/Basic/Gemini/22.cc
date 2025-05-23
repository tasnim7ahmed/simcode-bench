#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-mode.h"

#include <string>
#include <vector>
#include <map>

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("YansErrorRateModel", LOG_LEVEL_INFO);
    LogComponentEnable("NistErrorRateModel", LOG_LEVEL_INFO);
    LogComponentEnable("TableBasedErrorRateModel", LOG_LEVEL_INFO);

    uint32_t frameSize = 1500;
    double minSnr = 0.0;
    double maxSnr = 30.0;
    double snrStep = 0.5;

    std::vector<std::string> dsssModeStrings = {
        "DsssRate1Mbps",
        "DsssRate2Mbps",
        "CckRate5_5Mbps",
        "CckRate11Mbps"
    };

    std::vector<std::pair<WifiMode, std::string>> dsssModesAndNames;
    for (const auto& s : dsssModeStrings)
    {
        dsssModesAndNames.push_back({WifiMode(s), s});
    }

    Gnuplot plotYans("dsss_fsr_vs_snr_yans.eps", "Frame Success Rate vs. SNR (Yans Model)");
    plotYans.SetTitle("Frame Success Rate vs. SNR for DSSS Modes (Yans Model)");
    plotYans.SetXLabel("SNR (dB)");
    plotYans.SetYLabel("Frame Success Rate");
    plotYans.AppendExtra("set yrange [0:1]");
    plotYans.AppendExtra("set grid");

    Gnuplot plotNist("dsss_fsr_vs_snr_nist.eps", "Frame Success Rate vs. SNR (Nist Model)");
    plotNist.SetTitle("Frame Success Rate vs. SNR for DSSS Modes (Nist Model)");
    plotNist.SetXLabel("SNR (dB)");
    plotNist.SetYLabel("Frame Success Rate");
    plotNist.AppendExtra("set yrange [0:1]");
    plotNist.AppendExtra("set grid");

    Gnuplot plotTable("dsss_fsr_vs_snr_table.eps", "Frame Success Rate vs. SNR (Table Based Model)");
    plotTable.SetTitle("Frame Success Rate vs. SNR for DSSS Modes (Table Based Model)");
    plotTable.SetXLabel("SNR (dB)");
    plotTable.SetYLabel("Frame Success Rate");
    plotTable.AppendExtra("set yrange [0:1]");
    plotTable.AppendExtra("set grid");

    std::map<std::string, GnuplotDataset> yansDatasets;
    std::map<std::string, GnuplotDataset> nistDatasets;
    std::map<std::string, GnuplotDataset> tableDatasets;

    for (const auto& modeName : dsssModeStrings)
    {
        yansDatasets[modeName].SetTitle(modeName);
        yansDatasets[modeName].SetStyle("linespoints");

        nistDatasets[modeName].SetTitle(modeName);
        nistDatasets[modeName].SetStyle("linespoints");

        tableDatasets[modeName].SetTitle(modeName);
        tableDatasets[modeName].SetStyle("linespoints");
    }

    YansErrorRateModel yansModel;
    NistErrorRateModel nistModel;
    TableBasedErrorRateModel tableModel;

    for (const auto& modePair : dsssModesAndNames)
    {
        const WifiMode& currentMode = modePair.first;
        const std::string& modeName = modePair.second;
        
        for (double snr = minSnr; snr <= maxSnr; snr += snrStep)
        {
            double fsrYans = yansModel.GetFrameSuccessRate(snr, currentMode, currentMode, frameSize, 0.0);
            double fsrNist = nistModel.GetFrameSuccessRate(snr, currentMode, currentMode, frameSize, 0.0);
            double fsrTable = tableModel.GetFrameSuccessRate(snr, currentMode, currentMode, frameSize, 0.0);

            yansDatasets[modeName].Add(snr, fsrYans);
            nistDatasets[modeName].Add(snr, fsrNist);
            tableDatasets[modeName].Add(snr, fsrTable);
        }
    }

    for (const auto& modeName : dsssModeStrings)
    {
        plotYans.AddDataset(yansDatasets[modeName]);
        plotNist.AddDataset(nistDatasets[modeName]);
        plotTable.AddDataset(tableDatasets[modeName]);
    }

    plotYans.GenerateOutput();
    plotNist.GenerateOutput();
    plotTable.GenerateOutput();

    return 0;
}