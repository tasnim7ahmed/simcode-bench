#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DsssErrorRateValidation");

void
CalculateFrameSuccessRate(const WifiMode mode, const std::string& modelName, Gnuplot2dDataset& dataset)
{
    uint32_t frameSize = 1500; // bytes
    double snr;
    double successRate;
    uint8_t* pData = new uint8_t[frameSize];
    for (uint32_t i = 0; i < frameSize; ++i)
    {
        pData[i] = static_cast<uint8_t>(i % 256);
    }

    Ptr<ErrorRateModel> errorModel;

    if (modelName == "Yans")
    {
        errorModel = CreateObject<YansWifiErrorRateModel>();
    }
    else if (modelName == "Nist")
    {
        errorModel = CreateObject<NistWifiErrorRateModel>();
    }
    else if (modelName == "Table")
    {
        errorModel = CreateObject<TableBasedWifiErrorRateModel>();
    }

    for (int snrDb = -10; snrDb <= 30; snrDb += 2)
    {
        snr = std::pow(10.0, snrDb / 10.0); // Convert dB to linear
        successRate = errorModel->GetChunkSuccessRate(mode, snr, frameSize * 8);
        dataset.Add(snrDb, successRate);
    }

    delete[] pData;
}

int
main(int argc, char* argv[])
{
    uint32_t frameSize = 1500; // Default value, can be modified via command line if needed

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The size of the frame in bytes", frameSize);
    cmd.Parse(argc, argv);

    Gnuplot gnuplot("dsss-error-rate-validation.eps");
    gnuplot.SetTerminal("postscript eps color enhanced");
    gnuplot.SetTitle("Frame Success Rate vs SNR for DSSS Modes");
    gnuplot.SetLegend("SNR (dB)", "Frame Success Rate");
    gnuplot.SetExtra("set yrange [0:1]\nset grid");

    std::vector<std::string> models = {"Yans", "Nist", "Table"};
    std::vector<WifiMode> dsssModes;

    dsssModes.push_back(WifiPhy::GetDsssRate1Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate2Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate5_5Mbps());
    dsssModes.push_back(WifiPhy::GetDsssRate11Mbps());

    for (const auto& model : models)
    {
        for (const auto& mode : dsssModes)
        {
            Gnuplot2dDataset dataset;
            dataset.SetTitle(model + " - " + mode.GetUniqueName());
            CalculateFrameSuccessRate(mode, model, dataset);
            gnuplot.AddDataset(dataset);
        }
    }

    std::ofstream plotFile("dsss-error-rate-validation.plt");
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    return 0;
}