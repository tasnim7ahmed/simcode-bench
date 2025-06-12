#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRatePlot");

void CalculateAndPlotFrameSuccessRate(uint32_t frameSize, const std::string& plotFileName, const std::string& plotTitle) {
    Gnuplot plot(plotFileName);
    plot.SetTitle(plotTitle);
    plot.SetTerminal("png");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");
    plot.AppendExtra("set grid");

    std::map<std::string, WifiPhyBand> bands = {
        {"NIST", WIFI_PHY_BAND_UNSPECIFIED},
        {"YANS", WIFI_PHY_BAND_5GHZ},
        {"TableBased", WIFI_PHY_BAND_5GHZ}
    };

    for (auto it = bands.begin(); it != bands.end(); ++it) {
        std::string model = it->first;
        WifiPhyBand band = it->second;

        // Set error rate model
        Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(model + "ErrorRateModel"));

        WifiModeList vhtMcsList = WifiPhy::GetStaticModes();
        std::vector<Gnuplot2dDataset> datasets;

        for (uint8_t mcsIndex = 0; mcsIndex <= 9; mcsIndex++) {
            WifiMode mode = WifiPhy::GetVhtMcs(mcsIndex);
            if (mode.GetUniqueName() == "VhtMcs9" && (mcsIndex == 9)) continue; // Skip MCS 9 for 20 MHz

            datasets.emplace_back(Gnuplot2dDataset(mode.GetUniqueName()));
            datasets.back().SetStyle(Gnuplot2dDataset::LINES_POINTS);
        }

        double minSnrDb = -5.0;
        double maxSnrDb = 30.0;
        uint32_t numPoints = 40;

        for (uint32_t i = 0; i <= numPoints; ++i) {
            double snrDb = minSnrDb + (maxSnrDb - minSnrDb) * i / numPoints;
            double snr = std::pow(10.0, snrDb / 10.0);

            for (size_t j = 0; j < datasets.size(); ++j) {
                WifiMode mode = WifiPhy::GetVhtMcs(j);
                if (mode.GetUniqueName() == "VhtMcs9") continue;

                double successRate = 1.0 - WifiPhy::CalculatePe(mode, snr, frameSize * 8);
                datasets[j].Add(snrDb, successRate);
            }
        }

        for (const auto& dataset : datasets) {
            plot.AddDataset(dataset);
        }

        plot.GenerateOutput(std::cout);
    }
}

int main(int argc, char* argv[]) {
    uint32_t frameSize = 1500;
    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
    cmd.Parse(argc, argv);

    std::string plotFileName = "vht-error-rate-comparison-" + std::to_string(frameSize) + ".png";
    std::string plotTitle = "Frame Success Rate vs SNR for VHT MCS (Frame Size: " + std::to_string(frameSize) + " bytes)";

    CalculateAndPlotFrameSuccessRate(frameSize, plotFileName, plotTitle);

    return 0;
}