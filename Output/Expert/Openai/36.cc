#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"

using namespace ns3;

static std::vector<std::string> errorRateModels = {
    "ns3::NistErrorRateModel",
    "ns3::YansErrorRateModel",
    "ns3::TableBasedErrorRateModel"
};

static std::vector<std::string> errorRateNames = {
    "NIST",
    "YANS",
    "TABLE"
};

struct McsEntry {
    uint8_t mcs;
    bool valid;
};

static std::vector<McsEntry> GetVhtMcsEntries()
{
    // MCS 0-8 valid for 20 MHz; MCS 9 is not for VHT-20MHz
    std::vector<McsEntry> mcsList;
    for (uint8_t i = 0; i <= 8; ++i) {
        mcsList.push_back({i, true});
    }
    // If you wish to add MCS 9 for other bandwidths, customize here.
    return mcsList;
}

double
GetFrameSuccessRate(Ptr<WifiPhy> phy, Ptr<ErrorRateModel> model, WifiMode mode, double snrDb, uint32_t frameSize)
{
    double snrLinear = std::pow(10.0, snrDb / 10.0);

    // The rest of WifiPhy and ErrorRateModel interface is public
    WifiTxVector txVector;
    txVector.SetMode(mode);
    txVector.SetNss(1);
    txVector.SetChannelWidth(20);
    txVector.SetPreambleType(WIFI_PREAMBLE_VHT);
    txVector.SetGuardInterval(NORMAL_GUARD_INTERVAL);
    txVector.SetNUser(1);

    double per = model->GetChunkSuccessRate(mode, snrLinear, frameSize, txVector);
    return per;
}

int
main(int argc, char *argv[])
{
    uint32_t frameSize = 1200;
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    std::vector<McsEntry> mcsList = GetVhtMcsEntries();

    std::vector<Gnuplot> plots;

    for (uint32_t modelIdx = 0; modelIdx < errorRateModels.size(); ++modelIdx) {
        Gnuplot plot;
        plot.SetTitle("VHT Frame Success Rate vs SNR (" + errorRateNames[modelIdx] + ")");
        plot.SetLegend("SNR (dB)", "Frame Success Rate");
        plot.AppendExtra("set style data linespoints");
        plots.push_back(plot);
    }

    Ptr<YansWifiPhy> phy = CreateObject<YansWifiPhy>();
    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    phy->SetChannel(channel);

    std::vector<std::vector<Gnuplot2dDataset>> datasets(errorRateModels.size());

    for (uint32_t modelIdx = 0; modelIdx < errorRateModels.size(); ++modelIdx) {
        for (const auto& entry : mcsList) {
            std::ostringstream name;
            name << "MCS " << static_cast<unsigned>(entry.mcs);
            Gnuplot2dDataset dataset;
            dataset.SetTitle(name.str());
            dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

            // Set PHY error model
            phy->SetAttribute("ErrorRateModel", StringValue(errorRateModels[modelIdx]));
            Ptr<ErrorRateModel> model = phy->GetObject<ErrorRateModel>();
            WifiMode mode = WifiPhy::GetVhtMcs(entry.mcs);

            for (double snrDb = -5; snrDb <= 30; snrDb += 1.0) {
                double fsr = GetFrameSuccessRate(phy, model, mode, snrDb, frameSize);
                dataset.Add(snrDb, fsr);
            }
            datasets[modelIdx].push_back(dataset);
        }
    }

    for (uint32_t modelIdx = 0; modelIdx < errorRateModels.size(); ++modelIdx) {
        for (auto& dset : datasets[modelIdx]) {
            plots[modelIdx].AddDataset(dset);
        }
        std::ostringstream fname;
        fname << "vht-fsr-" << errorRateNames[modelIdx] << ".plt";
        std::ofstream plotFile(fname.str());
        plots[modelIdx].GenerateOutput(plotFile);
        plotFile.close();
    }

    Simulator::Destroy();
    return 0;
}