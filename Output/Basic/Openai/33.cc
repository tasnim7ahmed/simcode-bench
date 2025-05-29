#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1500;
    std::string dataDir = "./";
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("dataDir", "Output directory for plot data", dataDir);
    cmd.Parse(argc, argv);

    Gnuplot nistPlot((dataDir + "fse_vs_snr_nist.plt").c_str());
    Gnuplot yansPlot((dataDir + "fse_vs_snr_yans.plt").c_str());
    Gnuplot tablePlot((dataDir + "fse_vs_snr_table.plt").c_str());
    nistPlot.SetTitle("Frame Success Rate vs SNR (NIST)");
    yansPlot.SetTitle("Frame Success Rate vs SNR (YANS)");
    tablePlot.SetTitle("Frame Success Rate vs SNR (Table-based)");
    nistPlot.SetLegend("SNR (dB)", "Frame Success Rate");
    yansPlot.SetLegend("SNR (dB)", "Frame Success Rate");
    tablePlot.SetLegend("SNR (dB)", "Frame Success Rate");

    Ptr<WifiPhy> phy = CreateObject<YansWifiPhy>();
    WifiModeFactory factory;
    WifiPhyStandard standard = WIFI_PHY_STANDARD_80211n_5GHZ;
    phy->SetPhyStandard(standard);
    WifiHelper wifi;
    wifi.SetStandard(standard);

    // Get HT MCS modes (0 to 7, single stream)
    std::vector<WifiMode> htModes;
    for (uint8_t mcs = 0; mcs <= 7; ++mcs)
    {
        htModes.push_back(WifiPhy::GetHtMcs(mcs));
    }

    // Error rate models
    Ptr<ErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
    Ptr<ErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
    Ptr<ErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();

    std::vector<std::string> mcsLabels = {
        "HT MCS0", "HT MCS1", "HT MCS2", "HT MCS3",
        "HT MCS4", "HT MCS5", "HT MCS6", "HT MCS7"
    };

    // Plots for each MCS
    std::vector<Gnuplot2dDataset> nistDatasets;
    std::vector<Gnuplot2dDataset> yansDatasets;
    std::vector<Gnuplot2dDataset> tableDatasets;
    for (uint32_t m = 0; m < htModes.size(); ++m)
    {
        nistDatasets.emplace_back(mcsLabels[m]);
        yansDatasets.emplace_back(mcsLabels[m]);
        tableDatasets.emplace_back(mcsLabels[m]);
        nistDatasets.back().SetStyle(Gnuplot2dDataset::LINES_POINTS);
        yansDatasets.back().SetStyle(Gnuplot2dDataset::LINES_POINTS);
        tableDatasets.back().SetStyle(Gnuplot2dDataset::LINES_POINTS);
    }

    // SNR range (dB)
    double snrStart = 0.0, snrEnd = 35.0, snrStep = 1.0;
    double bandwidth = 20e6; // 20 MHz
    double frequency = 5e9;  // 5 GHz

    SpectrumWifiPhyHelper spectrumWifiPhyHelper;
    spectrumWifiPhyHelper.SetChannel(CreateObject<YansWifiChannel>());

    for (uint32_t m = 0; m < htModes.size(); ++m)
    {
        WifiMode mode = htModes[m];
        Ptr<const WifiPreamble> preamble = Create<WifiPreambleHtMixed>(); // HT-mixed
        for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
        {
            double snr = std::pow(10.0, snrDb / 10.0);

            // Use same error models for all
            double nistP = nistModel->GetChunkSuccessRate(mode, preamble, snr, frameSize * 8);
            double yansP = yansModel->GetChunkSuccessRate(mode, preamble, snr, frameSize * 8);
            double tableP = tableModel->GetChunkSuccessRate(mode, preamble, snr, frameSize * 8);

            nistDatasets[m].Add(snrDb, nistP);
            yansDatasets[m].Add(snrDb, yansP);
            tableDatasets[m].Add(snrDb, tableP);
        }
    }

    for (uint32_t m = 0; m < htModes.size(); ++m)
    {
        nistPlot.AddDataset(nistDatasets[m]);
        yansPlot.AddDataset(yansDatasets[m]);
        tablePlot.AddDataset(tableDatasets[m]);
    }

    std::ofstream nistPlotFile((dataDir + "fse_vs_snr_nist.plt").c_str());
    nistPlot.GenerateOutput(nistPlotFile);
    nistPlotFile.close();

    std::ofstream yansPlotFile((dataDir + "fse_vs_snr_yans.plt").c_str());
    yansPlot.GenerateOutput(yansPlotFile);
    yansPlotFile.close();

    std::ofstream tablePlotFile((dataDir + "fse_vs_snr_table.plt").c_str());
    tablePlot.GenerateOutput(tablePlotFile);
    tablePlotFile.close();

    std::cout << "Plots generated in " << dataDir << std::endl;
    return 0;
}