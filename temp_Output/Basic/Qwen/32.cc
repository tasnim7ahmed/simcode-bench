#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelsComparison");

void
PlotFrameSuccessRate(const std::string& plotFileName,
                      const std::string& plotTitle,
                      const std::map<std::string, std::vector<std::pair<double, double>>>& data)
{
    Gnuplot plot(plotFileName);
    plot.SetTitle(plotTitle);
    plot.SetTerminal("png");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");
    plot.AppendExtra("set grid");

    for (const auto& modelData : data)
    {
        Gnuplot2dDataset dataset(modelData.first);
        dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

        for (const auto& point : modelData.second)
        {
            dataset.Add(point.first, point.second);
        }

        plot.AddDataset(dataset);
    }

    std::ofstream plotFile(plotFileName + ".plt");
    plot.GenerateOutput(plotFile);
}

double
CalculateFsrForMcs(WifiTxVector txVector,
                   double snr,
                   uint32_t frameSize,
                   Ptr<ErrorRateModel> errorRateModel)
{
    WifiMode payloadMode = txVector.GetMode();
    WifiPreamble preamble = txVector.IsShortPreamble() ? WIFI_PREAMBLE_SHORT : WIFI_PREAMBLE_LONG;
    uint8_t numStreams = 1;

    double successProbability = 1.0;
    double ber = errorRateModel->GetChunkSuccessRate(payloadMode, txVector, snr, frameSize * 8);
    return ber;
}

int
main(int argc, char* argv[])
{
    uint32_t frameSize = 1472; // Default frame size in bytes
    double snrStart = 0;       // dB
    double snrEnd = 30;        // dB
    double snrStep = 2;        // dB

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The frame size in bytes", frameSize);
    cmd.AddValue("snrStart", "Starting SNR value in dB", snrStart);
    cmd.AddValue("snrEnd", "Ending SNR value in dB", snrEnd);
    cmd.AddValue("snrStep", "SNR step in dB", snrStep);
    cmd.Parse(argc, argv);

    std::vector<std::string> errorModels = {"Nist", "Yans", "Table"};
    std::map<std::string, std::vector<std::pair<double, double>>> results;

    for (const auto& model : errorModels)
    {
        Ptr<ErrorRateModel> errorModel;

        if (model == "Nist")
        {
            errorModel = CreateObject<NistErrorRateModel>();
        }
        else if (model == "Yans")
        {
            errorModel = CreateObject<YansErrorRateModel>();
        }
        else if (model == "Table")
        {
            errorModel = CreateObject<TableBasedErrorRateModel>();
        }

        for (uint8_t mcs = 0; mcs <= 11; ++mcs) // HE MCS values from 0 to 11
        {
            std::ostringstream key;
            key << model << "-MCS" << static_cast<int>(mcs);
            WifiTxVector txVector(
                WifiPhy::GetHeMcs(mcs), 0, WIFI_PREAMBLE_HE_SU, 800, 1, 1, 0, 20, false);
            for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
            {
                double fsr = CalculateFsrForMcs(txVector, snr, frameSize, errorModel);
                results[key.str()].emplace_back(snr, fsr);
            }
        }
    }

    PlotFrameSuccessRate("he_error_rate_models_comparison",
                         "Frame Success Rate vs SNR for HE MCS with Different Error Rate Models",
                         results);

    return 0;
}