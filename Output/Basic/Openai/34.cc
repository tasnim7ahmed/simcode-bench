#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[])
{
    double snrStart = 0.0;
    double snrEnd = 40.0;
    double snrStep = 1.0;
    uint32_t frameSize = 12000;

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    std::vector<std::string> models = { "Nist", "Yans", "Table" };
    std::map<std::string, Ptr<WifiPhy>> phyModels;
    std::map<std::string, Ptr<WifiErrorRateModel>> errorModels;

    Ptr<YansWifiPhy> refPhy = CreateObject<YansWifiPhy>();
    ptrdiff_t heMcsFirst = 0;
    ptrdiff_t heMcsLast = 11;

    Ptr<YansWifiPhy> phyNist = CreateObject<YansWifiPhy>();
    Ptr<YansWifiPhy> phyYans = CreateObject<YansWifiPhy>();
    Ptr<YansWifiPhy> phyTable = CreateObject<YansWifiPhy>();

    Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
    Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
    Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();

    phyNist->SetErrorRateModel(nistModel);
    phyYans->SetErrorRateModel(yansModel);
    phyTable->SetErrorRateModel(tableModel);

    phyModels["Nist"] = phyNist;
    phyModels["Yans"] = phyYans;
    phyModels["Table"] = phyTable;
    errorModels["Nist"] = nistModel;
    errorModels["Yans"] = yansModel;
    errorModels["Table"] = tableModel;

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);

    Ptr<WifiPhy> phy = CreateObject<YansWifiPhy>();

    WifiModeFactory modeFactory(WIFI_PHY_STANDARD_80211ax);
    std::vector<WifiMode> heModes;
    for (uint32_t mcs = heMcsFirst; mcs <= heMcsLast; ++mcs)
    {
        std::ostringstream oss;
        oss << "HeMcs" << mcs;
        heModes.push_back(modeFactory.CreateWifiMode(oss.str()));
    }

    std::vector<std::string> mcsLabels;
    for (uint32_t mcs = heMcsFirst; mcs <= heMcsLast; ++mcs)
    {
        std::ostringstream oss;
        oss << "HE-MCS" << mcs;
        mcsLabels.push_back(oss.str());
    }

    for (const auto& modelName : models)
    {
        std::string plotFile = "he-fsr-" + modelName + ".plt";
        std::string pngFile = "he-fsr-" + modelName + ".png";
        Gnuplot plot(plotFile);
        plot.SetTitle("Frame Success Rate vs SNR for " + modelName + " Error Model (HE Rates)");
        plot.SetLegend("SNR (dB)", "Frame Success Rate");
        plot.AppendExtra("set terminal png size 1024,768");
        plot.AppendExtra("set output '" + pngFile + "'");
        plot.AppendExtra("set grid");

        std::vector<Gnuplot2dDataset> datasets;
        for (uint32_t mcs = heMcsFirst; mcs <= heMcsLast; ++mcs)
        {
            Gnuplot2dDataset dataset;
            dataset.SetTitle(mcsLabels[mcs]);
            dataset.SetStyle(Gnuplot2dDataset::LINES);
            datasets.push_back(dataset);
        }

        for (double snr = snrStart; snr <= snrEnd + 1e-6; snr += snrStep)
        {
            for (uint32_t mcs = heMcsFirst; mcs <= heMcsLast; ++mcs)
            {
                double snrLinear = std::pow(10.0, snr / 10.0);
                WifiMode mode = heModes[mcs];
                double per = phyModels[modelName]->GetErrorRateModel()->GetChunkSuccessRate(
                    mode, snrLinear, frameSize, WifiPreamble::WIFI_PREAMBLE_HE_SU);
                double fsr = per;
                datasets[mcs].Add(snr, fsr);
            }
        }

        for (uint32_t mcs = heMcsFirst; mcs <= heMcsLast; ++mcs)
        {
            plot.AddDataset(datasets[mcs]);
        }
        std::ofstream plotFileStream(plotFile.c_str());
        plot.GenerateOutput(plotFileStream);
    }

    std::cout << "Simulation complete. Gnuplot files generated for each error model." << std::endl;
    return 0;
}