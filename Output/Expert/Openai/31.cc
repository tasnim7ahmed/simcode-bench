#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <iomanip>
#include <fstream>

using namespace ns3;

static double
FrameSuccessRate(Ptr<ErrorRateModel> errModel, WifiTxVector txVector, uint32_t frameSize, double snr)
{
    double per = errModel->CalculatePayloadSnrPer(txVector, frameSize, snr);
    return 1.0 - per;
}

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1200;
    double snrMin = 0.0;
    double snrMax = 40.0;
    double snrStep = 1.0;

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("snrMin", "Minimum SNR (dB)", snrMin);
    cmd.AddValue("snrMax", "Maximum SNR (dB)", snrMax);
    cmd.AddValue("snrStep", "SNR step (dB)", snrStep);
    cmd.Parse(argc, argv);

    WifiPhyStandard standard = WIFI_PHY_STANDARD_80211ax_5GHZ; // EHT not yet, ax-5GHz is equivalent for validation
    uint16_t channelWidth = 20;
    WifiTxVector txVector;
    txVector.SetChannelWidth(channelWidth);
    txVector.SetPreambleType(WIFI_PREAMBLE_HT_MF); // All rates are HT

    uint8_t mcsMax = 7;

    // Gnuplot setup
    Gnuplot plotNist("fsr_nist.eps");
    plotNist.SetTitle("NIST Frame Success Rate vs SNR");
    plotNist.SetTerminal("post eps enhanced color solid lw 2");
    plotNist.SetLegend("SNR (dB)", "Frame Success Rate");

    Gnuplot plotYans("fsr_yans.eps");
    plotYans.SetTitle("YANS Frame Success Rate vs SNR");
    plotYans.SetTerminal("post eps enhanced color solid lw 2");
    plotYans.SetLegend("SNR (dB)", "Frame Success Rate");

    Gnuplot plotTable("fsr_table.eps");
    plotTable.SetTitle("Table-based Model Frame Success Rate vs SNR");
    plotTable.SetTerminal("post eps enhanced color solid lw 2");
    plotTable.SetLegend("SNR (dB)", "Frame Success Rate");

    Ptr<WifiTxVectorFactory> txFactory = CreateObject<WifiTxVectorFactory>();
    txFactory->ConfigureStandard(standard, channelWidth);

    for (uint8_t mcs = 0; mcs <= mcsMax; ++mcs)
    {
        std::ostringstream label;
        label << "HT-MCS" << unsigned(mcs);

        Gnuplot2dDataset datasetNist(label.str());
        Gnuplot2dDataset datasetYans(label.str());
        Gnuplot2dDataset datasetTable(label.str());

        txVector.SetMode(WifiPhy::GetHtMcs(mcs));
        txVector.SetNss(1); // Single spatial stream

        Ptr<ErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
        Ptr<ErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
        Ptr<ErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();

        for (double snr = snrMin; snr <= snrMax; snr += snrStep)
        {
            double fsrNist = FrameSuccessRate(nistModel, txVector, frameSize, snr);
            double fsrYans = FrameSuccessRate(yansModel, txVector, frameSize, snr);
            double fsrTable = FrameSuccessRate(tableModel, txVector, frameSize, snr);

            datasetNist.Add(snr, fsrNist);
            datasetYans.Add(snr, fsrYans);
            datasetTable.Add(snr, fsrTable);
        }

        plotNist.AddDataset(datasetNist);
        plotYans.AddDataset(datasetYans);
        plotTable.AddDataset(datasetTable);
    }

    std::ofstream outFileNist("fsr_nist.plt");
    plotNist.GenerateOutput(outFileNist);
    outFileNist.close();

    std::ofstream outFileYans("fsr_yans.plt");
    plotYans.GenerateOutput(outFileYans);
    outFileYans.close();

    std::ofstream outFileTable("fsr_table.plt");
    plotTable.GenerateOutput(outFileTable);
    outFileTable.close();

    Simulator::Destroy();
    return 0;
}