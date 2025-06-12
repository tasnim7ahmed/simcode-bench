#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelsComparison");

void
GeneratePlotFiles()
{
    uint32_t frameSize = 1500; // bytes
    uint8_t mcsMax = 11;
    double snrMin = 0.0;     // dB
    double snrMax = 30.0;    // dB
    double snrStep = 1.0;    // dB

    GnuplotCollection gnuplots("he-error-rate-models-comparison");
    gnuplots.SetTerminal("png");

    for (uint8_t mcs = 0; mcs <= mcsMax; ++mcs)
    {
        std::ostringstream oss;
        oss << "HE-MCS" << static_cast<uint32_t>(mcs);
        Gnuplot plot(oss.str() + " Frame Success Rate vs SNR");
        plot.SetTitle(oss.str());
        plot.SetXTitle("SNR (dB)");
        plot.SetYTitle("Frame Success Rate");

        WifiTxVector txVector;
        txVector.SetMode(WifiModeFactory::CreateWifiMode("HeMcs" + std::to_string(mcs)));
        txVector.SetNss(1);

        Ptr<Packet> packet = Create<Packet>(frameSize);

        for (double snrDb = snrMin; snrDb <= snrMax; snrDb += snrStep)
        {
            double snr = std::pow(10.0, snrDb / 10.0); // Convert from dB to linear

            double fsrNist = 0.0;
            double fsrYans = 0.0;
            double fsrTable = 0.0;

            // NIST error rate model
            {
                Ptr<HePhy> phy = CreateObject<HePhy>();
                phy->SetErrorRateModel(CreateObject<Nist80211nMimoSnrToPer>());
                fsrNist = 1.0 - phy->CalculatePe(packet, txVector, snr, nullptr);
            }

            // YANS error rate model
            {
                Ptr<HePhy> phy = CreateObject<HePhy>();
                phy->SetErrorRateModel(CreateObject<YansErrorRateModel>());
                fsrYans = 1.0 - phy->CalculatePe(packet, txVector, snr, nullptr);
            }

            // Table-based error rate model
            {
                Ptr<HePhy> phy = CreateObject<HePhy>();
                phy->SetErrorRateModel(CreateObject<TableBasedErrorRateModel>());
                fsrTable = 1.0 - phy->CalculatePe(packet, txVector, snr, nullptr);
            }

            std::ostringstream datasetName;
            datasetName << "mcs" << static_cast<uint32_t>(mcs);
            plot.AddDataset(Gnuplot2dDataset(datasetName.str() + "-nist")).Add(snrDb, fsrNist);
            plot.AddDataset(Gnuplot2dDataset(datasetName.str() + "-yans")).Add(snrDb, fsrYans);
            plot.AddDataset(Gnuplot2dDataset(datasetName.str() + "-table")).Add(snrDb, fsrTable);
        }

        std::ostringstream plotFileName;
        plotFileName << "he-mcs" << static_cast<uint32_t>(mcs);
        gnuplots.AddPlot(plotFileName.str(), plot);
    }

    gnuplots.GenerateOutput();
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    GeneratePlotFiles();

    return 0;
}