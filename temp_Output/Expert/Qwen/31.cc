#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ErrorRatePlot");

double
CalculateFsrForMcs(Ptr<ErrorRateModel> errorRateModel, WifiMode mode, uint32_t frameSize, double snr)
{
    uint32_t payloadBits = frameSize * 8;
    double ps = errorRateModel->GetChunkSuccessRate(mode, snr, payloadBits);
    return ps;
}

int
main(int argc, char* argv[])
{
    uint32_t frameSize = 1472; // bytes
    double snrMin = 0.0;       // dB
    double snrMax = 30.0;      // dB
    double snrStep = 1.0;      // dB

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
    cmd.AddValue("snrMin", "Minimum SNR value (dB)", snrMin);
    cmd.AddValue("snrMax", "Maximum SNR value (dB)", snrMax);
    cmd.AddValue("snrStep", "SNR step size (dB)", snrStep);
    cmd.Parse(argc, argv);

    Gnuplot plotFsr = Gnuplot("error-rate-fsr-comparison.png");
    plotFsr.SetTitle("Frame Success Rate vs SNR for EHT MCS values");
    plotFsr.SetTerminal("png");
    plotFsr.SetLegend("SNR (dB)", "Frame Success Rate");
    plotFsr.AppendExtra("set grid");

    Gnuplot2dDataset datasetNist;
    datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    datasetNist.SetTitle("NIST Error Model");

    Gnuplot2dDataset datasetYans;
    datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    datasetYans.SetTitle("YANS Error Model");

    Gnuplot2dDataset datasetTable;
    datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    datasetTable.SetTitle("Table-based Error Model");

    Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
    Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
    Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();

    for (double snr = snrMin; snr <= snrMax; snr += snrStep)
    {
        double totalFsrNist = 0.0;
        double totalFsrYans = 0.0;
        double totalFsrTable = 0.0;
        uint32_t mcsCount = 0;

        for (uint8_t mcs = 0; mcs < 12; ++mcs) // HT MCS 0 to 11
        {
            WifiMode mode = WifiPhy::GetHtMcs(mcs);
            if (!mode.IsAllowed(HE_SU_PHY))
            {
                continue;
            }

            totalFsrNist += CalculateFsrForMcs(nistModel, mode, frameSize, DbToRatio(snr));
            totalFsrYans += CalculateFsrForMcs(yansModel, mode, frameSize, DbToRatio(snr));
            totalFsrTable += CalculateFsrForMcs(tableModel, mode, frameSize, DbToRatio(snr));
            ++mcsCount;
        }

        if (mcsCount > 0)
        {
            datasetNist.Add(snr, totalFsrNist / mcsCount);
            datasetYans.Add(snr, totalFsrYans / mcsCount);
            datasetTable.Add(snr, totalFsrTable / mcsCount);
        }
    }

    plotFsr.AddDataset(datasetNist);
    plotFsr.AddDataset(datasetYans);
    plotFsr.AddDataset(datasetTable);

    std::ofstream plotFile("error-rate-fsr-comparison.plt");
    plotFsr.GenerateOutput(plotFile);
    plotFile.close();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}