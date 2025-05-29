#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>

using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 1000;
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    std::vector<std::string> ofdmModes = {
        "OfdmRate6Mbps",
        "OfdmRate9Mbps",
        "OfdmRate12Mbps",
        "OfdmRate18Mbps",
        "OfdmRate24Mbps",
        "OfdmRate36Mbps",
        "OfdmRate48Mbps",
        "OfdmRate54Mbps"
    };

    Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
    Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
    Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();

    Gnuplot gnuplotNist("framerate_snr_nist.png");
    gnuplotNist.SetTitle("OFDM Frame Success Rate vs SNR - NIST Error Model");
    gnuplotNist.SetLegend("SNR [dB]", "Frame Success Rate");

    Gnuplot gnuplotYans("framerate_snr_yans.png");
    gnuplotYans.SetTitle("OFDM Frame Success Rate vs SNR - YANS Error Model");
    gnuplotYans.SetLegend("SNR [dB]", "Frame Success Rate");

    Gnuplot gnuplotTable("framerate_snr_table.png");
    gnuplotTable.SetTitle("OFDM Frame Success Rate vs SNR - Table-Based Error Model");
    gnuplotTable.SetLegend("SNR [dB]", "Frame Success Rate");

    for (const auto& modeName : ofdmModes)
    {
        WifiRemoteStationManager::Thresholds thresholds;
        WifiMode mode = WifiPhyHelper::Default()->GetPhyStandard () == WIFI_PHY_STANDARD_UNSPECIFIED 
            ? WifiMode(modeName.c_str()) 
            : WifiPhyHelper::Default()->FindMode(modeName);

        Gnuplot2dDataset datasetNist;
        datasetNist.SetTitle(modeName);
        datasetNist.SetStyle(Gnuplot2dDataset::LINES);

        Gnuplot2dDataset datasetYans;
        datasetYans.SetTitle(modeName);
        datasetYans.SetStyle(Gnuplot2dDataset::LINES);

        Gnuplot2dDataset datasetTable;
        datasetTable.SetTitle(modeName);
        datasetTable.SetStyle(Gnuplot2dDataset::LINES);

        for (double snrDb = -5.0; snrDb <= 30.0; snrDb += 1.0)
        {
            double snrLinear = pow(10.0, snrDb / 10.0);
            double noise = 1.0;
            double signal = snrLinear * noise;

            double berNist = nistModel->GetChunkSuccessRate(mode, signal, noise, frameSize * 8, WifiTxVector::CreateDefault(mode));
            double berYans = yansModel->GetChunkSuccessRate(mode, signal, noise, frameSize * 8, WifiTxVector::CreateDefault(mode));
            double berTable = tableModel->GetChunkSuccessRate(mode, signal, noise, frameSize * 8, WifiTxVector::CreateDefault(mode));

            datasetNist.Add(snrDb, berNist);
            datasetYans.Add(snrDb, berYans);
            datasetTable.Add(snrDb, berTable);
        }

        gnuplotNist.AddDataset(datasetNist);
        gnuplotYans.AddDataset(datasetYans);
        gnuplotTable.AddDataset(datasetTable);
    }

    std::ofstream outFileNist("framerate_snr_nist.plt");
    gnuplotNist.GenerateOutput(outFileNist);
    outFileNist.close();

    std::ofstream outFileYans("framerate_snr_yans.plt");
    gnuplotYans.GenerateOutput(outFileYans);
    outFileYans.close();

    std::ofstream outFileTable("framerate_snr_table.plt");
    gnuplotTable.GenerateOutput(outFileTable);
    outFileTable.close();

    return 0;
}