#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 12000; // bits
    double snrMin = 0.0;
    double snrMax = 35.0;
    double snrStep = 0.5;

    std::string outputFileName = "eht-error-models-fsr-vs-snr.plt";
    Gnuplot gnuplot (outputFileName);
    gnuplot.SetTitle ("Frame Success Rate vs SNR for EHT/HT MCS");
    gnuplot.SetLegend ("SNR (dB)", "Frame Success Rate");
    gnuplot.AppendExtra ("set yrange [0:1]");

    std::vector<std::string> modelLabels = {"Nist", "Yans", "Table"};
    std::vector<Ptr<ErrorRateModel>> errorModels;

    errorModels.push_back (CreateObject<NistErrorRateModel> ());
    errorModels.push_back (CreateObject<YansErrorRateModel> ());
    errorModels.push_back (CreateObject<TableBasedErrorRateModel> ());

    WifiPhyHeader header;
    // Iterate through all HT MCS values (0-7). EHT MCS not present in NS-3.41, fallback to HT for demonstration.
    WifiPhyStandard standard = WIFI_PHY_STANDARD_80211n_5GHZ;

    for (uint8_t mcs = 0; mcs <= 7; ++mcs)
    {
        // Create a plot for each MCS, each model has its own data set per MCS
        std::vector<Gnuplot2dDataset> datasets;
        for (const std::string &label : modelLabels)
        {
            Gnuplot2dDataset dataset;
            std::ostringstream oss;
            oss << label << " MCS " << unsigned(mcs);
            dataset.SetTitle(oss.str());
            dataset.SetStyle(Gnuplot2dDataset::LINES);
            datasets.push_back(dataset);
        }

        // Configure Wi-Fi mode for this MCS/standard
        WifiMode mode = WifiPhyHelper::GetHtMcs(standard, mcs);
        for (double snr = snrMin; snr <= snrMax; snr += snrStep)
        {
            int nBits = frameSize;
            for (size_t modelIdx = 0; modelIdx < errorModels.size(); ++modelIdx)
            {
                double per = errorModels[modelIdx]->GetChunkSuccessRate(mode, header, snr, nBits);
                datasets[modelIdx].Add(snr, per);
            }
        }
        for (size_t modelIdx = 0; modelIdx < datasets.size(); ++modelIdx)
        {
            gnuplot.AddDataset(datasets[modelIdx]);
        }
    }

    std::ofstream plotFile(outputFileName);
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    NS_LOG_UNCOND("Plot written to " << outputFileName);
    return 0;
}