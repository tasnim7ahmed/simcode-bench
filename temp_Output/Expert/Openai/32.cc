#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>

using namespace ns3;

static std::vector<uint8_t> GetHeMcsList()
{
    // 0-11 HE MCS
    std::vector<uint8_t> heMcs;
    for (uint8_t mcs = 0; mcs <= 11; ++mcs)
    {
        heMcs.push_back(mcs);
    }
    return heMcs;
}

static Ptr<ErrorRateModel>
CreateErrorRateModel(const std::string& modelName)
{
    if (modelName == "nist")
    {
        return CreateObject<NistErrorRateModel>();
    }
    else if (modelName == "yans")
    {
        return CreateObject<YansErrorRateModel>();
    }
    else if (modelName == "table")
    {
        return CreateObject<TableBasedErrorRateModel>();
    }
    return nullptr;
}

static std::string GetHeMcsName(uint8_t mcs)
{
    std::ostringstream oss;
    oss << "HE-MCS" << (unsigned)mcs;
    return oss.str();
}

int main(int argc, char* argv[])
{
    uint32_t frameSize = 1200;
    double snrMin = 0.0;
    double snrMax = 40.0;
    double snrStep = 1.0;
    std::string outputPrefix = "he-erfsr";
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("snrMin", "Minimum SNR in dB", snrMin);
    cmd.AddValue("snrMax", "Maximum SNR in dB", snrMax);
    cmd.AddValue("snrStep", "SNR step in dB", snrStep);
    cmd.AddValue("outputPrefix", "Output file prefix", outputPrefix);
    cmd.Parse(argc, argv);

    std::vector<std::string> errorModels = {"nist", "yans", "table"};
    std::vector<uint8_t> heMcsList = GetHeMcsList();

    WifiPhyStandard phyStandard = WIFI_PHY_STANDARD_80211ax_5GHZ;
    Ptr<YansWifiPhy> phy = CreateObject<YansWifiPhy>();
    phy->Set("ChannelWidth", UintegerValue(80));
    phy->Set("Frequency", UintegerValue(5180));
    phy->Set("Standard", EnumValue(phyStandard));

    WifiTxVector txVector;
    txVector.SetPreambleType(WIFI_PREAMBLE_HE_SU);

    uint16_t heNumSpatialStreams = 1; // Single stream

    for (const auto& errModelName : errorModels)
    {
        Gnuplot plot;
        plot.SetTitle("HE Frame Success Rate - " + errModelName);
        plot.SetLegend("SNR (dB)", "Frame Success Rate");

        for (auto mcs : heMcsList)
        {
            Gnuplot2dDataset dataset;
            dataset.SetTitle(GetHeMcsName(mcs));
            dataset.SetStyle(Gnuplot2dDataset::LINES);

            txVector.SetMode(WifiModeFactory::CreateHeMcs(mcs, heNumSpatialStreams));
            txVector.SetNss(heNumSpatialStreams);

            Ptr<ErrorRateModel> errModel = CreateErrorRateModel(errModelName);

            for (double snrDb = snrMin; snrDb <= snrMax; snrDb += snrStep)
            {
                double snrLinear = std::pow(10.0, snrDb / 10.0);

                Ptr<WifiPhy> tmpPhy = CreateObject<YansWifiPhy>();
                tmpPhy->SetErrorRateModel(errModel->Copy());
                tmpPhy->ConfigureStandard(phyStandard);

                WifiMode mode = txVector.GetMode();
                double ber = errModel->GetChunkSuccessRate(mode, heNumSpatialStreams, frameSize * 8, snrLinear);
                dataset.Add(snrDb, ber);

                tmpPhy = nullptr;
            }
            plot.AddDataset(dataset);
        }
        std::string plotFileName = outputPrefix + "-" + errModelName + ".plt";
        std::ofstream plotFile(plotFileName);
        plot.GenerateOutput(plotFile);
        plotFile.close();
    }

    Simulator::Destroy();
    return 0;
}