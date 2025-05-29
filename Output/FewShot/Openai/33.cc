#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <sstream>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1500;
    uint32_t snrStart = 0;
    uint32_t snrEnd = 40;
    double snrStep = 0.5;

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("snrStart", "Starting SNR value (dB)", snrStart);
    cmd.AddValue("snrEnd", "Ending SNR value (dB)", snrEnd);
    cmd.AddValue("snrStep", "SNR Step (dB)", snrStep);
    cmd.Parse(argc, argv);

    std::vector<Ptr<ErrorRateModel>> errorModels;
    std::vector<std::string> errorModelNames = { "Nist", "Yans", "Table" };
    errorModels.push_back(CreateObject<YansErrorRateModel>()); // For NIST model, use YansErrorRateModel::SetNist(true)
    errorModels.push_back(CreateObject<YansErrorRateModel>());
    errorModels.push_back(CreateObject<TableBasedErrorRateModel>());

    // Configure NIST
    DynamicCast<YansErrorRateModel>(errorModels[0])->SetNist(true);
    // Yans is default (with SetNist(false))
    DynamicCast<YansErrorRateModel>(errorModels[1])->SetNist(false);

    Gnuplot gnuplot("ht-fsr-vs-snr.png");
    gnuplot.SetTitle("HT Frame Success Rate vs SNR");
    gnuplot.SetTerminal("png");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");

    // Get all HT MCS modes for 802.11n (HT)
    WifiHelper wifi;
    uint16_t band = WIFI_PHY_STANDARD_80211n_5GHZ;
    WifiPhyStandard standard = WifiPhyStandard(band);
    WifiModeFactory modeFactory;
    WifiPhyHelper phyHelper = WifiPhyHelper::Default();
    wifi.SetStandard(standard);

    std::vector<WifiMode> htModes;
    WifiRemoteStationManagerHelper rsm;
    rsm.SetStandard(standard);
    // List all HT MCS from MCS 0 to 7 (single stream) and up to MCS 15 (4 streams, optional)
    for (uint8_t mcs = 0; mcs <= 7; ++mcs)
    {
        std::string modeStr = "HtMcs" + std::to_string(mcs);
        if (WifiModeFactory::IsRecognized(modeStr))
        {
            htModes.push_back(WifiMode(modeStr.c_str()));
        }
    }

    // Fallback for everything if the above fails: try manually generating the list from the mode factory
    if (htModes.empty())
    {
        for (uint8_t mcs = 0; mcs <= 7; ++mcs)
        {
            std::string modeStr = "HtMcs" + std::to_string(mcs);
            htModes.push_back(WifiMode(modeStr.c_str()));
        }
    }

    // Iterate over error models
    for (size_t em = 0; em < errorModels.size(); ++em)
    {
        Ptr<ErrorRateModel> err = errorModels[em];
        std::string errName = errorModelNames[em];

        // One dataset per MCS per error model
        for (size_t mi = 0; mi < htModes.size(); ++mi)
        {
            WifiMode mode = htModes[mi];
            std::ostringstream dataname;
            dataname << errName << "-MCS" << mi;
            Gnuplot2dDataset dataset(dataname.str());
            dataset.SetStyle(Gnuplot2dDataset::LINES);

            for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
            {
                double ber = err->GetChunkSuccessRate(mode, (double)frameSize * 8, snr); // Returns FSR
                dataset.Add(snr, ber);
            }
            gnuplot.AddDataset(dataset);
        }
    }

    std::ofstream plotFile("ht-fsr-vs-snr.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    std::cout << "FSR vs SNR data and plot for HT rates written to ht-fsr-vs-snr.plt / .png" << std::endl;
    return 0;
}