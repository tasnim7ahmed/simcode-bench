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
    uint32_t frameSize = 1200;
    std::string outputFileName = "ht-error-rate-validation.plt";
    CommandLine cmd;
    cmd.AddValue("FrameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("OutputFileName", "Name of Gnuplot output file", outputFileName);
    cmd.Parse(argc, argv);

    std::vector<std::string> modelNames = {"Nist", "Yans", "Table"};
    std::vector<Ptr<ErrorRateModel>> errorModels;
    errorModels.push_back(CreateObject<NistErrorRateModel>());
    errorModels.push_back(CreateObject<YansErrorRateModel>());
    errorModels.push_back(CreateObject<TableBasedErrorRateModel>());

    Ptr<WifiPhy> phy = CreateObject<YansWifiPhy>();
    WifiHelper wifi;
    WifiMacHelper mac;
    WifiPhyHelper phyHelper = WifiPhyHelper::Default();
    phyHelper.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11);

    std::vector<WifiMode> htModes;
    htModes.clear();
    WifiPhyStandard standard = WIFI_PHY_STANDARD_80211n_5GHZ;
    std::vector<std::string> mcsNames;
    for (uint8_t mcs = 0; mcs <= 7; ++mcs)
    {
        std::ostringstream oss;
        oss << "HtMcs" << unsigned(mcs);
        WifiMode mode = WifiPhy::GetHtMcs(oss.str());
        htModes.push_back(mode);
        mcsNames.push_back(oss.str());
    }

    double snrStart = 0.0;
    double snrEnd = 40.0;
    double snrStep = 1.0;
    std::vector<double> snrVector;
    for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
    {
        snrVector.push_back(snr);
    }

    Gnuplot plot(outputFileName);
    plot.SetTitle("Frame Success Rate vs SNR (HT, FrameSize=" + std::to_string(frameSize) + ")");
    plot.SetLegend("SNR [dB]", "Frame Success Rate");

    for (size_t modelIdx = 0; modelIdx < errorModels.size(); ++modelIdx)
    {
        Ptr<ErrorRateModel> model = errorModels[modelIdx];
        std::string modelName = modelNames[modelIdx];

        for (size_t mcsIdx = 0; mcsIdx < htModes.size(); ++mcsIdx)
        {
            WifiMode mode = htModes[mcsIdx];
            std::string datasetName = modelName + " " + mcsNames[mcsIdx];
            Gnuplot2dDataset dataset;
            dataset.SetTitle(datasetName);
            dataset.SetStyle(Gnuplot2dDataset::LINES);

            double bandwidth = 20.0e6;
            double noiseFigure = 7.0; // dB, as default

            for (auto snrDb : snrVector)
            {
                double snrLinear = std::pow(10.0, snrDb / 10.0);
                double noise = 290.0 * 1.38064852e-23 * bandwidth * std::pow(10.0, noiseFigure / 10.0);
                double txPower = 1.0; // unit power
                double rxPower = snrLinear * noise;
                double snr = rxPower / noise;

                double per = model->GetChunkSuccessRate(mode, &phy->GetChannel(), snrDb, frameSize * 8);
                double fsr = per;
                if (per < 0.0)
                    fsr = 0.0;
                else if (per > 1.0)
                    fsr = 1.0;

                dataset.Add(snrDb, fsr);
            }
            plot.AddDataset(dataset);
        }
    }
    std::ofstream plotFile(outputFileName.c_str());
    plot.GenerateOutput(plotFile);
    plotFile.close();
    Simulator::Destroy();
    return 0;
}