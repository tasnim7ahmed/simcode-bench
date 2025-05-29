#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-phy.h"
#include "ns3/he-phy.h"
#include "ns3/wifi-utils.h"
#include "ns3/he-mcs.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1200; // default Ethernet frame size in bytes
    double snrStart = 0.0;     // Start SNR (dB)
    double snrEnd = 40.0;      // End SNR (dB)
    double snrStep = 1.0;      // SNR step (dB)

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("snrStart", "Start SNR (dB)", snrStart);
    cmd.AddValue("snrEnd", "End SNR (dB)", snrEnd);
    cmd.AddValue("snrStep", "SNR step (dB)", snrStep);
    cmd.Parse(argc, argv);

    // Prepare Gnuplot output
    std::vector<std::string> modelNames = {"NIST", "YANS", "TABLE"};
    std::vector<Ptr<ErrorRateModel>> errorModels;
    errorModels.push_back(CreateObject<NistErrorRateModel>());
    errorModels.push_back(CreateObject<YansErrorRateModel>());
    Ptr<TableBasedErrorRateModel> tableErm = CreateObject<TableBasedErrorRateModel>();
    tableErm->LoadDefaultTables();
    errorModels.push_back(tableErm);

    // Get supported HE MCSs in NS-3 (MCS 0-11 for single spatial stream)
    std::vector<std::pair<uint8_t, uint8_t>> heMcsList;
    for (uint8_t mcsIndex = 0; mcsIndex <= 11; ++mcsIndex)
    {
        heMcsList.emplace_back(mcsIndex, 1); // (MCS, NSS=1)
    }

    for (size_t modelIdx = 0; modelIdx < errorModels.size(); ++modelIdx)
    {
        std::string modelName = modelNames[modelIdx];
        Gnuplot plot("he-" + modelName + "-fsr-vs-snr.png");
        plot.SetTitle("HE FSR vs SNR (" + modelName + " Model)");
        plot.SetLegend("SNR (dB)", "Frame Success Rate (FSR)");
        
        for (const auto &mcsPair : heMcsList)
        {
            uint8_t mcs = mcsPair.first;
            uint8_t nss = mcsPair.second;

            // MCS configuration
            WifiTxVector txVector;
            txVector.SetMode(WifiPhy::GetHeMode());
            txVector.SetChannelWidth(20);
            txVector.SetNss(nss);
            txVector.SetHeMcs(mcs);

            std::ostringstream label;
            label << "HE MCS " << unsigned(mcs);

            Gnuplot2dDataset dataset(label.str());
            dataset.SetStyle(Gnuplot2dDataset::LINES);

            for (double snrDb = snrStart; snrDb <= snrEnd; snrDb += snrStep)
            {
                double snr = std::pow(10.0, snrDb / 10.0);

                WifiMode mode = WifiHeMcsToWifiMode(mcs, nss);
                // Use 6 GHz (as HE supports 2.4/5/6GHz; result differences are in symbol time, etc.)
                // but for FEC, modulation, they are similar
               
                double ber = errorModels[modelIdx]->GetChunkSuccessRate(
                    mode,
                    snr,
                    frameSize * 8);
                dataset.Add(snrDb, ber);
            }
            plot.AddDataset(dataset);
        }

        std::string plotFilename = "he-fsr-vs-snr-" + modelName + ".plt";
        std::ofstream plotFile(plotFilename);
        plot.GenerateOutput(plotFile);
        plotFile.close();
    }

    std::cout << "Gnuplot scripts written. To plot, run: gnuplot he-fsr-vs-snr-<MODEL>.plt" << std::endl;
    return 0;
}