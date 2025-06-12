#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRateModelComparison");

std::vector<std::string> errorModels = {"Nist", "Yans", "Table"};
uint32_t frameSize = 1500;

void CalculateAndPlotFrameSuccessRate()
{
    GnuplotCollection gnuplots("vht-error-rate-comparison.plt");
    gnuplots.SetTitle("Frame Success Rate vs SNR for VHT MCS");
    gnuplots.SetTerminal("png");

    for (const auto& model : errorModels)
    {
        WifiPhyBand band = WIFI_PHY_BAND_5GHZ;
        uint8_t nss = 1;
        uint16_t channelWidth = 20;

        std::ostringstream oss;
        oss << "Frame Success Rate (" << model << ", " << frameSize << " bytes)";
        Gnuplot plot(oss.str());
        plot.SetLegend("SNR (dB)", "Frame Success Rate");
        plot.SetExtra("set key bottom right");

        for (double snrDb = -5; snrDb <= 30; snrDb += 1.0)
        {
            double snr = std::pow(10.0, snrDb / 10.0);

            for (uint8_t mcsIndex = 0; mcsIndex <= 9; ++mcsIndex)
            {
                WifiModulationClass modulationClass = WIFI_MOD_CLASS_VHT;
                WifiMode mode = WifiModeFactory::CreateWifiMode("VhtMcs" + std::to_string(mcsIndex), modulationClass, false, false);
                WifiTxVector txVector;
                txVector.SetMode(mode);
                txVector.SetNss(nss);
                txVector.SetChannelWidth(channelWidth);
                txVector.SetGuardInterval(800); // default GI

                if (channelWidth == 20 && mcsIndex == 9)
                {
                    continue; // Skip MCS 9 for 20 MHz
                }

                uint32_t totalBits = mode.GetDataRate(txVector, band) * Seconds(1e-3); // bits per millisecond
                uint32_t frameBits = frameSize * 8;
                uint32_t numSymbols = ceil(static_cast<double>(frameBits) / mode.GetBitsPerSymbol(txVector.IsShortGuardInterval(), txVector.GetChannelWidth()));

                double ber = 0.0;
                if (model == "Nist")
                {
                    ber = NistErrorRateModel().GetFer(snr, mode, frameBits, txVector, band);
                }
                else if (model == "Yans")
                {
                    ber = YansErrorRateModel().GetFer(snr, mode, frameBits, txVector, band);
                }
                else if (model == "Table")
                {
                    ber = TableBasedErrorRateModel().GetFer(snr, mode, frameBits, txVector, band);
                }

                double fsr = 1.0 - ber;

                std::ostringstream datasetName;
                datasetName << "MCS" << static_cast<uint32_t>(mcsIndex);
                plot.AppendExtra("set style line 1 lc rgb 'black' pt 1 ps 1");
                plot.AddDataset(Gnuplot2dDataset(datasetName.str()));
                size_t index = plot.GetNDatasets() - 1;
                plot.GetDataset(index).Add(snrDb, fsr);
            }
        }

        gnuplots.AddPlot(plot);
    }

    gnuplots.GenerateOutput();
}

int main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
    cmd.Parse(argc, argv);

    CalculateAndPlotFrameSuccessRate();

    return 0;
}