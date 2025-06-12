#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1200;
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    // SNR range (dB)
    double snrStart = 0.0;
    double snrEnd = 40.0;
    double snrStep = 1.0;

    // Get all HE MCS combinations for 802.11ax (HE)
    WifiHelper wifi;
    WifiMacHelper mac;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    WifiRemoteStationManagerHelper staman;
    WifiModeFactory modeFactory;

    // Get all HE MCS modes for single stream, 20 MHz, normal GI
    std::vector<WifiMode> heModes;
    std::vector<std::string> validRates = WifiPhy::GetValidRates("Ht","He");
    std::vector<WifiMode> allModes = WifiPhy::GetHeModes();

    for (WifiMode mode : allModes)
    {
        if (mode.GetModulationClass() == WIFI_MOD_CLASS_HE && mode.GetNumberOfStreams() == 1)
        {
            // 20 MHz, normal GI only
            if (mode.GetChannelWidth() == 20 && mode.GetGuardInterval() == 800)
            {
                heModes.push_back(mode);
            }
        }
    }

    // For each error rate model: NIST, YANS, Table
    struct ModelInfo
    {
        std::string name;
        Ptr<ErrorRateModel> model;
    };

    Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
    Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
    Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();
    // Precompute YANS and NIST lookup tables for efficiency
    nistModel->InitializeLookupTables();
    yansModel->InitializeLookupTables();

    std::vector<ModelInfo> models = {
        {"NIST", nistModel},
        {"YANS", yansModel},
        {"Table", tableModel}
    };

    // Prepare printers
    std::vector<Gnuplot> gnuplots;
    for (const auto &m : models)
    {
        Gnuplot plot((m.name + ".plt").c_str());
        plot.SetTitle("Frame Success Rate vs SNR (" + m.name + " Model)");
        plot.SetLegend("SNR (dB)", "Frame Success Rate");
        plot.SetTerminal("png");
        plot.SetExtra("set yrange [0:1]");
        gnuplots.push_back(plot);
    }

    // Prepare Gnuplot datasets: [model][mode index]
    std::vector<std::vector<Gnuplot2dDataset>> datasets(
        models.size(), std::vector<Gnuplot2dDataset>(heModes.size()));

    for (uint32_t m = 0; m < models.size(); ++m)
    {
        for (uint32_t i = 0; i < heModes.size(); ++i)
        {
            std::ostringstream oss;
            oss << "HE MCS " << heModes[i].GetMcsValue();
            datasets[m][i].SetTitle(oss.str());
            datasets[m][i].SetStyle(Gnuplot2dDataset::LINES);
        }
    }

    // Frame size in bits
    uint32_t frameSizeBits = 8 * frameSize;

    // Calculate FSR (Frame Success Rate)
    for (double snrDb = snrStart; snrDb <= snrEnd + 0.0001; snrDb += snrStep)
    {
        double snr = std::pow(10.0, snrDb / 10.0);
        for (uint32_t i = 0; i < heModes.size(); ++i)
        {
            const WifiMode &mode = heModes[i];
            for (uint32_t m = 0; m < models.size(); ++m)
            {
                double per = models[m].model->GetChunkSuccessRate(mode, snr, frameSizeBits);
                // NS-3 error models return "success rate" (not error rate) for GetChunkSuccessRate
                double fsr = per;
                datasets[m][i].Add(snrDb, fsr);
            }
        }
    }

    // Attach datasets and generate plot files
    for (uint32_t m = 0; m < models.size(); ++m)
    {
        for (uint32_t i = 0; i < heModes.size(); ++i)
        {
            gnuplots[m].AddDataset(datasets[m][i]);
        }
        std::ofstream plotfile((models[m].name + ".plt").c_str());
        gnuplots[m].GenerateOutput(plotfile);
        plotfile.close();
    }

    return 0;
}