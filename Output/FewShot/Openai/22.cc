#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <cmath>
#include <iomanip>

using namespace ns3;

// DSSS rates and modulation: 1 Mbps (DBPSK), 2 Mbps (DQPSK), 5.5 Mbps (CCK), 11 Mbps (CCK)
struct DsssMode
{
    std::string name;
    WifiMode mode;
    double rateMbps;
};

static std::vector<DsssMode> dsssModes;

void InitDsssModes()
{
    dsssModes.clear();
    dsssModes.push_back({"1 Mbps (DBPSK)", WifiPhyStandard::WIFI_PHY_STANDARD_80211b, 1.0});
    dsssModes.push_back({"2 Mbps (DQPSK)", WifiPhyStandard::WIFI_PHY_STANDARD_80211b, 2.0});
    dsssModes.push_back({"5.5 Mbps (CCK)", WifiPhyStandard::WIFI_PHY_STANDARD_80211b, 5.5});
    dsssModes.push_back({"11 Mbps (CCK)", WifiPhyStandard::WIFI_PHY_STANDARD_80211b, 11.0});
}

struct ModelInfo
{
    Ptr<ErrorRateModel> model;
    std::string name;
};

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1024; // bytes
    std::string outFile = "fsr-vs-snr-dsss.eps";

    InitDsssModes();

    // Prepare Gnuplot
    Gnuplot plot(outFile);
    plot.SetTitle("DSSS Frame Success Rate vs SNR");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");
    plot.SetTerminal("postscript eps color enhanced");
    plot.AppendExtra("set yrange [0:1]");

    // SNR sweep parameters
    double snrStart = 0;
    double snrEnd = 25;
    double snrStep = 0.5;

    // Prepare models: Yans, Nist, Table-based
    std::vector<ModelInfo> models;
    models.push_back({CreateObject<YansErrorRateModel>(), "Yans"});
    models.push_back({CreateObject<NistErrorRateModel>(), "Nist"});
    models.push_back({CreateObject<TableBasedErrorRateModel>(), "TableBased"});

    // Iterate over DSSS modes
    for (const auto &dsss : dsssModes)
    {
        // One dataset per model and mode
        for (const auto &mod : models)
        {
            std::ostringstream dsLabel;
            dsLabel << mod.name << " - " << dsss.name;
            Gnuplot2dDataset dataset(dsLabel.str());
            dataset.SetStyle(Gnuplot2dDataset::LINES);

            // Loop through SNRs
            for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
            {
                // Rx and noise powers (linear scale)
                double rx = 1.0; // arbitrary reference
                double noise = rx / std::pow(10.0, snr / 10.0);

                // Pick wifi mode
                WifiPhyHelper wifiPhy = WifiPhyHelper::Default();
                wifiPhy.SetChannel(CreateObject<YansWifiChannel>());
                wifiPhy.Set("Standard", StringValue("802.11b"));

                WifiMode mode;
                if      (dsss.rateMbps == 1.0)  mode = WifiPhyHelper::GetDsssRate1Mbps();
                else if (dsss.rateMbps == 2.0)  mode = WifiPhyHelper::GetDsssRate2Mbps();
                else if (dsss.rateMbps == 5.5)  mode = WifiPhyHelper::GetDsssRate5_5Mbps();
                else if (dsss.rateMbps == 11.0) mode = WifiPhyHelper::GetDsssRate11Mbps();
                else continue;

                // Calculate frame error rate
                double fer = mod.model->GetChunkSuccessRate(mode, frameSize * 8, snr, 1);
                double fsr = 1.0 - fer;

                // Validation for reasonable range
                if (fsr < 0) fsr = 0;
                if (fsr > 1) fsr = 1;

                dataset.Add(snr, fsr);
            }
            plot.AddDataset(dataset);
        }
    }

    // Output plot to file
    std::ofstream plotFile(outFile);
    plot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}