// This example is to show difference among Nist, Yans and Table-based error rate models.
//
// It outputs plots of the Frame Error Rate versus the Signal-to-noise ratio for
// Nist, Yans and Table-based error rate models and for MCS 0, 4 and 7 value.

#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/yans-error-rate-model.h"

#include <cmath>
#include <fstream>

using namespace ns3;

int
main(int argc, char* argv[])
{
    uint32_t size = 1500 * 8; // bits
    bool tableErrorModelEnabled = true;
    bool yansErrorModelEnabled = true;
    bool nistErrorModelEnabled = true;
    uint8_t beginMcs = 0;
    uint8_t endMcs = 7;
    uint8_t stepMcs = 4;
    std::string format("Ht");

    CommandLine cmd(__FILE__);
    cmd.AddValue("size", "The size in bits", size);
    cmd.AddValue("frameFormat", "The frame format to use: Ht, Vht or He", format);
    cmd.AddValue("beginMcs", "The first MCS to test", beginMcs);
    cmd.AddValue("endMcs", "The last MCS to test", endMcs);
    cmd.AddValue("stepMcs", "The step between two MCSs to test", stepMcs);
    cmd.AddValue("includeTableErrorModel",
                 "Flag to include/exclude Table-based error model",
                 tableErrorModelEnabled);
    cmd.AddValue("includeYansErrorModel",
                 "Flag to include/exclude Yans error model",
                 yansErrorModelEnabled);
    cmd.AddValue("includeNistErrorModel",
                 "Flag to include/exclude Nist error model",
                 nistErrorModelEnabled);
    cmd.Parse(argc, argv);

    std::ofstream errormodelfile("wifi-error-rate-models.plt");
    Gnuplot plot = Gnuplot("wifi-error-rate-models.eps");

    Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
    Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
    Ptr<TableBasedErrorRateModel> table = CreateObject<TableBasedErrorRateModel>();
    WifiTxVector txVector;
    std::vector<std::string> modes;

    std::stringstream mode;
    mode << format << "Mcs" << +beginMcs;
    modes.push_back(mode.str());
    for (uint8_t mcs = (beginMcs + stepMcs); mcs < endMcs; mcs += stepMcs)
    {
        mode.str("");
        mode << format << "Mcs" << +mcs;
        modes.push_back(mode.str());
    }
    mode.str("");
    mode << format << "Mcs" << +endMcs;
    modes.push_back(mode.str());

    for (const auto& mode : modes)
    {
        std::cout << mode << std::endl;
        Gnuplot2dDataset yansdataset(mode);
        Gnuplot2dDataset nistdataset(mode);
        Gnuplot2dDataset tabledataset(mode);
        txVector.SetMode(mode);

        WifiMode wifiMode(mode);

        for (double snrDb = -5.0; snrDb <= (endMcs * 5); snrDb += 0.1)
        {
            double snr = std::pow(10.0, snrDb / 10.0);

            double ps = yans->GetChunkSuccessRate(wifiMode, txVector, snr, size);
            if (ps < 0 || ps > 1)
            {
                // error
                exit(1);
            }
            yansdataset.Add(snrDb, 1 - ps);
            ps = nist->GetChunkSuccessRate(wifiMode, txVector, snr, size);
            if (ps < 0 || ps > 1)
            {
                // error
                exit(1);
            }
            nistdataset.Add(snrDb, 1 - ps);
            ps = table->GetChunkSuccessRate(wifiMode, txVector, snr, size);
            if (ps < 0 || ps > 1)
            {
                // error
                exit(1);
            }
            tabledataset.Add(snrDb, 1 - ps);
        }

        if (tableErrorModelEnabled)
        {
            std::stringstream ss;
            ss << "Table-" << mode;
            tabledataset.SetTitle(ss.str());
            plot.AddDataset(tabledataset);
        }
        if (yansErrorModelEnabled)
        {
            std::stringstream ss;
            ss << "Yans-" << mode;
            yansdataset.SetTitle(ss.str());
            plot.AddDataset(yansdataset);
        }
        if (nistErrorModelEnabled)
        {
            std::stringstream ss;
            ss << "Nist-" << mode;
            nistdataset.SetTitle(ss.str());
            plot.AddDataset(nistdataset);
        }
    }

    plot.SetTerminal("postscript eps color enh \"Times-BoldItalic\"");
    plot.SetLegend("SNR(dB)", "Frame Error Rate");

    std::stringstream plotExtra;
    plotExtra << "set xrange [-5:" << endMcs * 5 << "]\n\
set log y\n\
set yrange [0.0001:1]\n";

    uint8_t lineNumber = 1;
    for (uint32_t i = 0; i < modes.size(); i++)
    {
        if (tableErrorModelEnabled)
        {
            plotExtra << "set style line " << +lineNumber++
                      << " linewidth 5 linecolor rgb \"red\" \n";
        }
        if (yansErrorModelEnabled)
        {
            plotExtra << "set style line " << +lineNumber++
                      << " linewidth 5 linecolor rgb \"green\" \n";
        }
        if (nistErrorModelEnabled)
        {
            plotExtra << "set style line " << +lineNumber++
                      << " linewidth 5 linecolor rgb \"blue\" \n";
        }
    }

    plotExtra << "set style increment user";
    plot.SetExtra(plotExtra.str());

    plot.GenerateOutput(errormodelfile);
    errormodelfile.close();

    return 0;
}
