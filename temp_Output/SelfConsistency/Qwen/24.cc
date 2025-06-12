#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRateComparison");

double
CalculateFerForModel(Ptr<ErrorRateModel> errorModel, WifiMode mode, double snr, uint32_t size)
{
    Ptr<Packet> packet = Create<Packet>(size);
    WifiTxVector txVector;
    txVector.SetMode(mode);

    double ps = errorModel->GetChunkSuccessRate(mode, snr, txVector, packet->GetSize());
    return (1.0 - ps);
}

int
main(int argc, char* argv[])
{
    std::string frameSizeStr = "1472"; // bytes
    std::string mcsValuesStr = "0,4,7";
    std::string snrMinStr = "0";
    std::string snrMaxStr = "30";
    std::string snrStepStr = "2";
    bool enableNist = true;
    bool enableYans = true;
    bool enableTable = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "Frame size in bytes", frameSizeStr);
    cmd.AddValue("mcsValues", "Comma-separated list of MCS values to test", mcsValuesStr);
    cmd.AddValue("snrMin", "Minimum SNR value (dB)", snrMinStr);
    cmd.AddValue("snrMax", "Maximum SNR value (dB)", snrMaxStr);
    cmd.AddValue("snrStep", "SNR step value (dB)", snrStepStr);
    cmd.AddValue("enableNist", "Enable Nist Error Rate Model", enableNist);
    cmd.AddValue("enableYans", "Enable Yans Error Rate Model", enableYans);
    cmd.AddValue("enableTable", "Enable Table-based Error Rate Model", enableTable);
    cmd.Parse(argc, argv);

    uint32_t frameSize = std::stoi(frameSizeStr);
    double snrMin = std::stod(snrMinStr);
    double snrMax = std::stod(snrMaxStr);
    double snrStep = std::stod(snrStepStr);

    std::vector<uint8_t> mcsValues;
    std::istringstream mcsStream(mcsValuesStr);
    std::string token;
    while (std::getline(mcsStream, token, ','))
    {
        mcsValues.push_back(static_cast<uint8_t>(std::stoi(token)));
    }

    GnuplotCollection gnuplot("error-rate-comparison.eps");
    gnuplot.SetTerminal("postscript eps color enhanced");
    gnuplot.SetTitle("Frame Error Rate Comparison");
    gnuplot.SetLegend("SNR (dB)", "FER");

    for (auto mcs : mcsValues)
    {
        WifiMode mode = WifiPhy::GetOfdmRateFromMcs(mcs);
        if (!mode.IsDefined())
        {
            NS_LOG_WARN("Invalid MCS value: " << static_cast<uint32_t>(mcs));
            continue;
        }

        Gnuplot2dDataset datasetNist("MCS " + std::to_string(mcs) + " - Nist");
        Gnuplot2dDataset datasetYans("MCS " + std::to_string(mcs) + " - Yans");
        Gnuplot2dDataset datasetTable("MCS " + std::to_string(mcs) + " - Table");

        datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
        datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);

        datasetNist.SetColor(2 * mcs + 1);
        datasetYans.SetColor(2 * mcs + 2);
        datasetTable.SetColor(2 * mcs + 3);

        for (double snr = snrMin; snr <= snrMax; snr += snrStep)
        {
            if (enableNist)
            {
                static Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
                double fer = CalculateFerForModel(nistModel, mode, DbToRatio(snr), frameSize);
                datasetNist.Add(snr, fer);
            }

            if (enableYans)
            {
                static Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
                double fer = CalculateFerForModel(yansModel, mode, DbToRatio(snr), frameSize);
                datasetYans.Add(snr, fer);
            }

            if (enableTable)
            {
                static Ptr<TableBasedErrorRateModel> tableModel =
                    CreateObject<TableBasedErrorRateModel>();
                double fer = CalculateFerForModel(tableModel, mode, DbToRatio(snr), frameSize);
                datasetTable.Add(snr, fer);
            }
        }

        if (enableNist)
            gnuplot.AddDataset(datasetNist);
        if (enableYans)
            gnuplot.AddDataset(datasetYans);
        if (enableTable)
            gnuplot.AddDataset(datasetTable);
    }

    gnuplot.GenerateOutput(File("error-rate-comparison.plt"));
    Simulator::Destroy();

    return 0;
}