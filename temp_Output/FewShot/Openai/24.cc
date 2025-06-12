#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"

using namespace ns3;

static double GetFer(Ptr<ErrorRateModel> model, WifiTxVector txVector, uint32_t frameSize, double snr_db)
{
    double snr_linear = std::pow(10.0, snr_db / 10.0);
    double ber = model->GetChunkSuccessRate(txVector, snr_linear, frameSize * 8);
    double fer = 1.0 - ber;
    return fer;
}

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1200;
    uint32_t mcsStart = 0;
    uint32_t mcsStop = 7;
    uint32_t mcsStep = 4;
    double snrStart = 0.0;
    double snrStop = 30.0;
    double snrStep = 2.0;
    bool enableNist = true;
    bool enableYans = true;
    bool enableTable = true;
    std::string plotFileName = "fer_vs_snr.eps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("mcsStart", "Start MCS value", mcsStart);
    cmd.AddValue("mcsStop", "Stop MCS value", mcsStop);
    cmd.AddValue("mcsStep", "Step for MCS values", mcsStep);
    cmd.AddValue("snrStart", "Start SNR in dB", snrStart);
    cmd.AddValue("snrStop", "Stop SNR in dB", snrStop);
    cmd.AddValue("snrStep", "SNR step in dB", snrStep);
    cmd.AddValue("enableNist", "Enable NIST error model", enableNist);
    cmd.AddValue("enableYans", "Enable YANS error model", enableYans);
    cmd.AddValue("enableTable", "Enable Table-Based error model", enableTable);
    cmd.AddValue("plotFileName", "Plot output file (.eps)", plotFileName);
    cmd.Parse(argc, argv);

    std::vector<uint32_t> mcsValues;
    for (uint32_t m = mcsStart; m <= mcsStop; m += mcsStep)
        mcsValues.push_back(m);

    std::vector<double> snrValues;
    for (double snr = snrStart; snr <= snrStop + 1e-6; snr += snrStep)
        snrValues.push_back(snr);

    std::vector<std::string> modelNames = { "NIST", "YANS", "Table" };
    std::vector<std::string> modelColors = { "red", "blue", "green" };

    // Gnuplot setup
    Gnuplot plot(plotFileName);
    plot.SetTitle("Frame Error Rate vs SNR");
    plot.SetTerminal("postscript eps enhanced color font 'Helvetica,18' size 8,6");
    plot.SetLegend("SNR (dB)", "FER");
    plot.AppendExtra("set yrange [1e-5:1]");
    plot.AppendExtra("set logscale y");

    for (size_t mcsIdx = 0; mcsIdx < mcsValues.size(); ++mcsIdx)
    {
        uint32_t mcs = mcsValues[mcsIdx];

        // Set up TX vector for 802.11n (HT) for MCS
        WifiTxVector txVector;
        txVector.SetMode(WifiPhyHelper::GetHtSupportedMcs(mcs == 7 ? 7 : mcs));
        txVector.SetNss(1);
        txVector.SetShortGuardInterval(false);
        txVector.SetChannelWidth(20);

        // NIST Error Model
        if (enableNist)
        {
            Ptr<ErrorRateModel> nist = CreateObject<NistErrorRateModel>();
            Gnuplot2dDataset dataSet("Nist MCS " + std::to_string(mcs));
            dataSet.SetStyle(Gnuplot2dDataset::LINES);
            dataSet.SetLineWidth(2);
            dataSet.SetColour("red");
            for (double snrDb : snrValues)
            {
                double fer = GetFer(nist, txVector, frameSize, snrDb);
                if (fer < 1e-8) fer = 1e-8;
                dataSet.Add(snrDb, fer);
            }
            plot.AddDataset(dataSet);
        }
        // YANS Error Model
        if (enableYans)
        {
            Ptr<ErrorRateModel> yans = CreateObject<YansErrorRateModel>();
            Gnuplot2dDataset dataSet("Yans MCS " + std::to_string(mcs));
            dataSet.SetStyle(Gnuplot2dDataset::LINES);
            dataSet.SetLineWidth(2);
            dataSet.SetColour("blue");
            for (double snrDb : snrValues)
            {
                double fer = GetFer(yans, txVector, frameSize, snrDb);
                if (fer < 1e-8) fer = 1e-8;
                dataSet.Add(snrDb, fer);
            }
            plot.AddDataset(dataSet);
        }
        // Table-Based Error Model
        if (enableTable)
        {
            Ptr<ErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
            Gnuplot2dDataset dataSet("Table MCS " + std::to_string(mcs));
            dataSet.SetStyle(Gnuplot2dDataset::LINES);
            dataSet.SetLineWidth(2);
            dataSet.SetColour("green");
            for (double snrDb : snrValues)
            {
                double fer = GetFer(table, txVector, frameSize, snrDb);
                if (fer < 1e-8) fer = 1e-8;
                dataSet.Add(snrDb, fer);
            }
            plot.AddDataset(dataSet);
        }
    }

    std::ofstream plotFile(plotFileName.c_str());
    plot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}