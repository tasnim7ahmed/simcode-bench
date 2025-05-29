#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t frameSize = 12000; // bytes
    double snrStart = 0.0;
    double snrEnd = 30.0;
    double snrStep = 1.0;

    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.AddValue("snrStart", "SNR start (dB)", snrStart);
    cmd.AddValue("snrEnd", "SNR end (dB)", snrEnd);
    cmd.AddValue("snrStep", "SNR step (dB)", snrStep);
    cmd.Parse(argc, argv);

    std::vector<double> snrDbVec;
    for (double snr = snrStart; snr <= snrEnd + 1e-6; snr += snrStep)
    {
        snrDbVec.push_back(snr);
    }

    std::vector<std::string> modelNames = {
        "Nist", "Yans", "Table"
    };

    Ptr<WifiPhy> phy = CreateObject<YansWifiPhy>(); // Dummy, not used

    uint8_t nMcs = 12; // EHT has MCS 0-11

    Gnuplot gnuplot("fsr-vs-snr.eps");
    gnuplot.SetTitle("Frame Success Rate vs SNR (EHT MCS)");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");

    for (uint8_t mcs = 0; mcs < nMcs; ++mcs)
    {
        std::vector<Gnuplot2dDataset> datasets;
        for (const auto& model : modelNames)
        {
            std::ostringstream dsName;
            dsName << model << " MCS" << unsigned(mcs);
            Gnuplot2dDataset ds(dsName.str());
            ds.SetStyle(Gnuplot2dDataset::LINES);

            Ptr<ErrorRateModel> errorRate;
            if (model == "Nist")
            {
                errorRate = CreateObject<NistErrorRateModel>();
            }
            else if (model == "Yans")
            {
                errorRate = CreateObject<YansErrorRateModel>();
            }
            else
            {
                errorRate = CreateObject<TableBasedErrorRateModel>();
            }

            for (double snrDb : snrDbVec)
            {
                double snrLinear = std::pow(10.0, snrDb / 10.0);
                WifiTxVector txVector;
                txVector.SetMode(WifiPhy::GetHeMcs(mcs)); // EHT uses HEMCS
                txVector.SetNss(1);
                txVector.SetChannelWidth(20);
                txVector.SetGuardInterval(NANOSECONDS(800)); // 0.8us

                // Compute frame error rate
                double error = errorRate->GetChunkSuccessRate(
                    wifiModulationClass(GetHeModulationClass(txVector.GetMode())),
                    txVector, phy,
                    snrLinear, frameSize * 8, WifiPreamble::HE_SU);
                double frameSuccessRate = error;

                ds.Add(snrDb, frameSuccessRate);
            }
            datasets.push_back(ds);
        }
        for (auto& ds : datasets)
        {
            gnuplot.AddDataset(ds);
        }
    }

    std::ofstream file("fsr-vs-snr.plt");
    gnuplot.GenerateOutput(file);
    std::cout << "Plotted Frame Success Rate vs SNR for Nist, Yans, Table error models (EHT MCS 0-11)." << std::endl;
    std::cout << "Gnuplot script: fsr-vs-snr.plt" << std::endl;

    return 0;
}