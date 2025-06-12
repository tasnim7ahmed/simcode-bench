#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DsssErrorRateValidation");

void
CalculateFrameSuccessRate(const WifiMode& mode, const std::string& modelName, Gnuplot2dDataset& dataset)
{
    Ptr<ErrorRateModel> error = nullptr;

    if (modelName == "Yans")
    {
        error = CreateObject<YansWifiErrorRateModel>();
    }
    else if (modelName == "Nist")
    {
        error = CreateObject<NistWifiErrorRateModel>();
    }
    else if (modelName == "Table")
    {
        error = CreateObject<TableBasedWifiErrorRateModel>();
    }
    else
    {
        NS_FATAL_ERROR("Unknown error rate model: " << modelName);
    }

    uint32_t frameSize = 1500; // bytes

    for (double snrDb = 0; snrDb <= 20; snrDb += 1.0)
    {
        double ps = 0.0;
        if (mode.GetModulationClass() == WIFI_MOD_CLASS_DSSS)
        {
            ps = error->GetChunkSuccessRate(mode, DbmToRatio(snrDb - 30), frameSize * 8, 0);
        }
        else
        {
            NS_LOG_WARN("Skipping non-DSSS mode: " << mode.GetUniqueName());
            continue;
        }

        double successRate = ps;
        if (successRate < 0.0)
        {
            successRate = 0.0;
        }
        else if (successRate > 1.0)
        {
            successRate = 1.0;
        }

        dataset.Add(snrDb, successRate);
    }
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("DsssErrorRateValidation", LOG_LEVEL_INFO);

    std::vector<std::string> models = {"Yans", "Nist", "Table"};
    std::vector<WifiMode> dsssModes;

    dsssModes.push_back(WifiPhy::GetDsssRate1Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate2Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate5_5Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate11Mbps());

    Gnuplot gnuplot("dsss-error-rate-validation.eps");
    gnuplot.SetTerminal("postscript eps color enhanced");
    gnuplot.SetTitle("DSSS Frame Success Rate vs SNR");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.SetExtra("set yrange [0:1.05]\nset key below");

    for (const auto& mode : dsssModes)
    {
        for (const auto& model : models)
        {
            Gnuplot2dDataset dataset;
            dataset.SetTitle(model + " - " + mode.GetUniqueName());
            dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

            CalculateFrameSuccessRate(mode, model, dataset);

            gnuplot.AddDataset(dataset);
        }
    }

    std::ofstream plotFile("dsss-error-rate-validation.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    Simulator::Destroy();
    return 0;
}