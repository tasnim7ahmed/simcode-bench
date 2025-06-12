#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelComparison");

void
GeneratePlotFiles()
{
    std::string plotTitle = "Frame Success Rate vs SNR for HE Rates";
    Gnuplot gnuplot(plotTitle);
    gnuplot.SetTerminal("png");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.SetExtra("set key outside\nset grid");

    Gnuplot2dDataset datasetNist;
    datasetNist.SetTitle("NIST");
    datasetNist.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset datasetYans;
    datasetYans.SetTitle("YANS");
    datasetYans.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset datasetTable;
    datasetTable.SetTitle("Table-Based");
    datasetTable.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    // Assign datasets to the plot
    gnuplot.AddDataset(datasetNist);
    gnuplot.AddDataset(datasetYans);
    gnuplot.AddDataset(datasetTable);

    std::ofstream plotFile("he-error-rate-comparison.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();
}

int
main(int argc, char* argv[])
{
    uint32_t frameSize = 1500; // Default frame size in bytes

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
    cmd.Parse(argc, argv);

    Ptr<WifiPhy> phy = CreateObject<WifiPhy>();
    phy->SetOperatingChannel(WifiPhyOperatingChannel{20, 6, WIFI_PHY_BAND_5GHZ, HT_MODE});

    WifiModeList heModes = WifiPhy::GetStaticHtHeOoklaModeList();

    std::vector<double> snrDbList;
    for (double snrDb = 0; snrDb <= 30; snrDb += 2)
    {
        snrDbList.push_back(snrDb);
    }

    ErrorRateModel::ErrorRateType errorRateTypes[] = {
        ErrorRateModel::ERROR_RATE_TYPE_NIST,
        ErrorRateModel::ERROR_RATE_TYPE_YANS,
        ErrorRateModel::ERROR_RATE_TYPE_TABLE};

    std::map<std::string, Gnuplot2dDataset> datasets;
    datasets["nist"].SetTitle("NIST");
    datasets["yans"].SetTitle("YANS");
    datasets["table"].SetTitle("Table-Based");

    for (const auto& type : errorRateTypes)
    {
        std::string modelName;
        switch (type)
        {
        case ErrorRateModel::ERROR_RATE_TYPE_NIST:
            modelName = "nist";
            break;
        case ErrorRateModel::ERROR_RATE_TYPE_YANS:
            modelName = "yans";
            break;
        case ErrorRateModel::ERROR_RATE_TYPE_TABLE:
            modelName = "table";
            break;
        }

        std::ostringstream filename;
        filename << "he-" << modelName << "-fsr-vs-snr.dat";
        std::ofstream file(filename.str());

        for (double snrDb : snrDbList)
        {
            double totalFsr = 0.0;
            uint32_t numModes = 0;

            for (const auto& mode : heModes)
            {
                if (mode.GetModulationClass() != WIFI_MOD_CLASS_HE)
                {
                    continue;
                }

                phy->SetErrorRateModel(type);
                double ber = phy->CalculateBer(mode, Seconds(1), snrDb, frameSize * 8);
                double fsr = std::pow((1 - ber), (frameSize * 8));
                totalFsr += fsr;
                numModes++;
            }

            if (numModes > 0)
            {
                totalFsr /= numModes;
            }

            file << snrDb << " " << totalFsr << std::endl;
            datasets[modelName].Add(snrDb, totalFsr);
        }

        file.close();
    }

    GeneratePlotFiles();

    return 0;
}