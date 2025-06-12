#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <iomanip>
#include <sstream>

using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 12000; // in bits
    double snrStart = 0.0;
    double snrStop = 40.0; // dB
    double snrStep = 1.0;
    std::string outputFileNameWithNoExtension = "eht-error-model-compare";
    uint8_t ehtMcsMax = 11; // EHT MCS index 0-11 for 802.11be

    CommandLine cmd;
    cmd.AddValue("frameSize", "Size of frame in bits", frameSize);
    cmd.AddValue("snrStart", "SNR start value [dB]", snrStart);
    cmd.AddValue("snrStop", "SNR stop value [dB]", snrStop);
    cmd.AddValue("snrStep", "SNR step [dB]", snrStep);
    cmd.AddValue("outputFileName", "Gnuplot output base filename", outputFileNameWithNoExtension);
    cmd.AddValue("ehtMcsMax", "Maximum EHT MCS index (inclusive, up to 11)", ehtMcsMax);
    cmd.Parse(argc, argv);

    // EHT MCS Table mapping to WifiModes
    Ptr<WifiPhy> phy = CreateObject<YansWifiPhy>();
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be_EHT);

    std::vector<Ptr<ErrorRateModel> > errorModels;
    errorModels.push_back(CreateObject<NistErrorRateModel>());
    errorModels.push_back(CreateObject<YansErrorRateModel>());
    errorModels.push_back(CreateObject<TableBasedErrorRateModel>());

    std::vector<std::string> modelNames;
    modelNames.push_back("NistErrorRateModel");
    modelNames.push_back("YansErrorRateModel");
    modelNames.push_back("TableBasedErrorRateModel");

    Gnuplot plot (outputFileNameWithNoExtension + ".plt");
    plot.SetTitle ("Frame Success Rate vs SNR for EHT Rates");
    plot.SetTerminal ("png");
    plot.SetLegend ("SNR (dB)", "Frame Success Rate");

    for (uint8_t mcs = 0; mcs <= ehtMcsMax; ++mcs)
    {
        std::ostringstream label;
        label << "EHT-MCS-" << unsigned(mcs);

        Gnuplot2dDataset dsNist(label.str() + " (Nist)");
        dsNist.SetStyle(Gnuplot2dDataset::LINES);

        Gnuplot2dDataset dsYans(label.str() + " (Yans)");
        dsYans.SetStyle(Gnuplot2dDataset::LINES);

        Gnuplot2dDataset dsTable(label.str() + " (Table)");
        dsTable.SetStyle(Gnuplot2dDataset::LINES);

        // Find the mode for EHT MCS (using 80 MHz, 1 spatial stream)
        WifiPhyStandard standard = WIFI_STANDARD_80211be_EHT;
        uint8_t nss = 1;
        std::vector<WifiMode> ehtModes;
        WifiPhy::GetEhtMcsList(ehtModes, standard, 80, false, nss);

        if (mcs >= ehtModes.size())
            continue;

        WifiMode mode = ehtModes.at(mcs);

        for (double snrDb = snrStart; snrDb <= snrStop + 0.1; snrDb += snrStep)
        {
            double snrLinear = pow(10.0, snrDb / 10.0);

            double per[3];
            for (size_t model = 0; model < errorModels.size(); ++model)
            {
                Ptr<ErrorRateModel> perror = errorModels[model];
                double thisPer = perror->GetChunkSuccessRate(
                        mode, (uint32_t) (frameSize / mode.GetModulation().GetCodeRate()),
                        snrLinear, 1 // nTx
                );
                if (thisPer < 0.0)
                    thisPer = 0.0;
                if (thisPer > 1.0)
                    thisPer = 1.0;
                per[model] = thisPer;
            }
            dsNist.Add(snrDb, per[0]);
            dsYans.Add(snrDb, per[1]);
            dsTable.Add(snrDb, per[2]);
        }
        plot.AddDataset(dsNist);
        plot.AddDataset(dsYans);
        plot.AddDataset(dsTable);
    }

    std::ofstream plotFile(outputFileNameWithNoExtension + ".plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();

    std::cout << "Plot generated: " << outputFileNameWithNoExtension << ".plt" << std::endl;
    std::cout << "Convert with: gnuplot " << outputFileNameWithNoExtension << ".plt" << std::endl;

    return 0;
}