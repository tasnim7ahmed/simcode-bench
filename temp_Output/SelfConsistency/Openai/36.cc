#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VhtErrorRateValidation");

struct ModelDesc {
    std::string name;
    Ptr<ErrorRateModel> (*createModel)();
};

Ptr<ErrorRateModel> CreateNistModel() {
    return CreateObject<NistErrorRateModel>();
}

Ptr<ErrorRateModel> CreateYansModel() {
    return CreateObject<YansErrorRateModel>();
}

Ptr<ErrorRateModel> CreateTableModel() {
    return CreateObject<TableBasedErrorRateModel>();
}

static const std::vector<ModelDesc> errorRateModels = {
    {"NIST",  &CreateNistModel},
    {"YANS",  &CreateYansModel},
    {"Table", &CreateTableModel},
};

// Return vector of VHT WifiModes for 20 MHz, short GI, 1 spatial stream, excluding MCS 9.
std::vector<WifiMode> GetVhtModes()
{
    WifiPhyStandard standard = WIFI_PHY_STANDARD_80211ac;
    WifiModeFactory *factory = WifiModeFactory::GetFactoryForStandard (standard);
    std::vector<WifiMode> vhtModes;
    // Valid VHT MCS: 0-8 (exclude 9 for 20 MHz)
    for (uint32_t mcs = 0; mcs <= 8; ++mcs)
    {
        // Corresponds to: width=20MHz, NSS=1, SGI=false (or true)
        // In NS-3, WifiMode names reflect these properties.
        // We'll get the VHT rate for NSS=1, GI=short, channel width 20 MHz
        std::ostringstream os;
        // Mode name pattern: "VhtMcs{mcs}"
        os << "VhtMcs" << mcs;
        if (factory->SearchMode (os.str()) != WifiMode())
            vhtModes.push_back (factory->SearchMode (os.str()));
    }
    return vhtModes;
}

double CalculateFrameSuccessRate(Ptr<ErrorRateModel> model, WifiMode mode, double snr, uint32_t frameSize)
{
    // Assume QAM, AWGN channel, no interference.
    double ber = model->GetChunkSuccessRate (mode, snr, frameSize, nullptr);
    return ber;
}

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1200;
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    std::vector<WifiMode> vhtModes = GetVhtModes();
    std::vector<std::string> mcsLabels;
    for (uint32_t mcs = 0; mcs < vhtModes.size(); ++mcs)
    {
        std::ostringstream oss;
        oss << "MCS " << mcs;
        mcsLabels.push_back(oss.str());
    }

    double snrStart = -5.0;
    double snrEnd   = 30.0;
    double snrStep  = 1.0;

    for (const auto& modelDesc : errorRateModels)
    {
        Gnuplot plot((modelDesc.name + "-VHT-FrameSuccessRate.png").c_str());
        plot.SetTitle(modelDesc.name + " VHT Frame Success Rate vs SNR");
        plot.SetLegend("SNR (dB)", "Frame Success Rate");
        plot.AppendExtra("set yrange [0:1]");
        
        std::vector<Gnuplot2dDataset> datasets(vhtModes.size());

        for (uint32_t mi = 0; mi < vhtModes.size(); ++mi)
        {
            datasets[mi].SetTitle(mcsLabels[mi]);
            datasets[mi].SetStyle(Gnuplot2dDataset::LINES);
        }

        for (double snr = snrStart; snr <= snrEnd; snr += snrStep)
        {
            Ptr<ErrorRateModel> errorModel = modelDesc.createModel();
            for (uint32_t mi = 0; mi < vhtModes.size(); ++mi)
            {
                WifiMode mode = vhtModes[mi];
                // FrameSuccessRate = P(success transmitting 'frameSize' bits)
                double fsr = CalculateFrameSuccessRate(errorModel, mode, snr, frameSize*8);
                datasets[mi].Add(snr, fsr);
            }
        }

        for (uint32_t mi = 0; mi < vhtModes.size(); ++mi)
        {
            plot.AddDataset(datasets[mi]);
        }

        std::ofstream plotFile((modelDesc.name + "-VHT-FrameSuccessRate.plt").c_str());
        plot.GenerateOutput(plotFile);
        plotFile.close();

        std::cout << "Generated " << modelDesc.name << " plot to "
                  << (modelDesc.name + "-VHT-FrameSuccessRate.plt") << std::endl;
    }

    return 0;
}