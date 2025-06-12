#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HtErrorRateValidation");

void GenerateFrameSuccessRatePlot(uint32_t frameSize) {
    // Define MCS values for HT (0 to 31)
    const uint8_t maxMcs = 31;
    const uint32_t numSnrs = 20;
    const double snrStep = 2.0;  // dB
    const double startSnr = -5.0;
    const double endSnr = startSnr + snrStep * (numSnrs - 1);

    std::vector<std::string> models = {"Nist", "Yans", "Table"};
    std::map<std::string, Ptr<ErrorRateModel>> ermMap;

    ermMap["Nist"] = CreateObject<NistErrorRateModel>();
    ermMap["Yans"] = CreateObject<YansErrorRateModel>();
    ermMap["Table"] = CreateObject<TableBasedErrorRateModel>();

    WifiModulationClass modulationClass = WIFI_MOD_CLASS_HT;

    for (uint8_t mcs = 0; mcs <= maxMcs; ++mcs) {
        std::ostringstream plotFileName;
        plotFileName << "ht-mcs-" << static_cast<uint32_t>(mcs) << "-framesize-" << frameSize << "-fsrc-vs-snr.plt";
        Gnuplot gnuplot(plotFileName.str());
        gnuplot.SetTitle("Frame Success Rate vs SNR for HT MCS " + std::to_string(mcs));
        gnuplot.SetXTitle("SNR (dB)");
        gnuplot.SetYTitle("Frame Success Rate");

        for (const auto& model : models) {
            Gnuplot2Ddataset dataset;
            dataset.SetTitle(model.first);
            dataset.SetStyle(Gnuplot2Ddataset::LINES_POINTS);

            for (uint32_t i = 0; i < numSnrs; ++i) {
                double snrDb = startSnr + i * snrStep;
                double snr = std::pow(10.0, snrDb / 10.0); // Convert dB to linear

                WifiMode mode = WifiUtils::GetHtWifiMode(mcs, 0, false, false, false, modulationClass);
                WifiTxVector txVector(mode, 0, WIFI_PREAMBLE_HT_MF, 800, 1, 1, 0, 20, false);

                double ps = 0.0;
                for (uint32_t size = 0; size < frameSize; size++) {
                    ps += ermMap[model.first]->GetChunkSuccessRate(mode, snr, 1);
                }
                double fsrc = ps / frameSize;
                dataset.Add(snrDb, fsrc);
            }

            gnuplot.AddDataset(dataset);
        }

        std::ostringstream outputFilename;
        outputFilename << "ht-mcs-" << static_cast<uint32_t>(mcs) << "-framesize-" << frameSize << "-fsrc-vs-snr";
        gnuplot.GenerateOutput(File(outputFilename.str() + ".plt"), File(outputFilename.str() + ".eps"));
    }
}

int main(int argc, char *argv[]) {
    uint32_t frameSize = 1472;

    CommandLine cmd;
    cmd.AddValue("frameSize", "The frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);

    GenerateFrameSuccessRatePlot(frameSize);

    return 0;
}