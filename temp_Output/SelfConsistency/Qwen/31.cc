#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/propagation-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EhtErrorRateModelComparison");

double
CalculateFrameSuccessRate(Ptr<WifiPhy> phy, WifiTxVector txVector, double snr, uint32_t frameSize)
{
    double successRate = 1.0;
    uint32_t payloadBits = frameSize * 8;
    for (uint32_t i = 0; i < payloadBits; i++)
    {
        double bitSuccess = phy->GetErrorRateModel()->GetChunkSuccessRate(phy, txVector, snr, 1);
        successRate *= bitSuccess;
    }
    return successRate;
}

int
main(int argc, char* argv[])
{
    uint32_t frameSize = 1500; // bytes
    double snrMin = 0;         // dB
    double snrMax = 30;        // dB
    double snrStep = 2;        // dB

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
    cmd.AddValue("snrMin", "Minimum SNR value in dB", snrMin);
    cmd.AddValue("snrMax", "Maximum SNR value in dB", snrMax);
    cmd.AddValue("snrStep", "SNR step in dB", snrStep);
    cmd.Parse(argc, argv);

    Gnuplot gnuplot("eht-error-rate-comparison-frams-success-rate.png");
    gnuplot.SetTitle("Frame Success Rate vs SNR for EHT MCS values");
    gnuplot.SetTerminal("png");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.SetExtra("set grid");

    Gnuplot2dDataset yansDataset("Yans Error Rate Model");
    yansDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset nistDataset("Nist Error Rate Model");
    nistDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset tableDataset("Table-based Error Rate Model");
    tableDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    // Create a generic Phy to configure error rate models
    Ptr<WifiPhy> phy = CreateObject<WifiPhy>();

    WifiTxVector txVector;
    txVector.SetPreambleType(WIFI_PREAMBLE_EHT_MU);
    txVector.SetChannelWidth(80);
    txVector.SetGuardInterval(800);

    for (double snrDb = snrMin; snrDb <= snrMax; snrDb += snrStep)
    {
        double snr = std::pow(10.0, snrDb / 10.0); // Convert dB to linear

        double avgYans = 0.0;
        double avgNist = 0.0;
        double avgTable = 0.0;

        uint8_t numMcs = 13; // HT MCS indices from 0 to 12

        for (uint8_t mcs = 0; mcs < numMcs; ++mcs)
        {
            txVector.SetMode(WifiModeFactory::CreateWifiMode(
                "http://www.nsnam.org/doxygen/class_wifi_mode.html",
                WIFI_MOD_CLASS_HT,
                false,
                false,
                WIFI_PREAMBLE_HT_MF,
                20,
                mcs,
                650)); // arbitrary rate for HT MCS

            // Yans model
            phy->SetErrorRateModel(CreateObject<YansErrorRateModel>());
            avgYans += CalculateFrameSuccessRate(phy, txVector, snr, frameSize);

            // Nist model
            phy->SetErrorRateModel(CreateObject<NistErrorRateModel>());
            avgNist += CalculateFrameSuccessRate(phy, txVector, snr, frameSize);

            // Table-based model
            phy->SetErrorRateModel(CreateObject<TableBasedErrorRateModel>());
            avgTable += CalculateFrameSuccessRate(phy, txVector, snr, frameSize);
        }

        avgYans /= numMcs;
        avgNist /= numMcs;
        avgTable /= numMcs;

        yansDataset.Add(snrDb, avgYans);
        nistDataset.Add(snrDb, avgNist);
        tableDataset.Add(snrDb, avgTable);
    }

    gnuplot.AddDataset(yansDataset);
    gnuplot.AddDataset(nistDataset);
    gnuplot.AddDataset(tableDataset);

    std::ofstream plotFile("eht-error-rate-comparison.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    Simulator::Destroy();
    return 0;
}