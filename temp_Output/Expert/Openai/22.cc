#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Configurable parameters
    uint32_t frameSize = 1024; // bytes
    std::string outputFileName = "frr_vs_snr.eps";
    std::string plotTitle = "DSSS Frame Success Rate vs SNR";
    std::string plotXLabel = "SNR (dB)";
    std::string plotYLabel = "Frame Success Rate";

    // Gnuplot setup
    Gnuplot gnuplot(outputFileName, plotTitle);
    gnuplot.SetTerminal("postscript eps color enhanced");
    gnuplot.SetLegend(plotXLabel, plotYLabel);
    gnuplot.AppendExtra("set xrange [0:25]");
    gnuplot.AppendExtra("set yrange [0:1]");
    gnuplot.AppendExtra("set key left top");

    // SNR values and DSSS modes
    std::vector<double> snrDb = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24};
    struct ModeDesc
    {
        WifiMode mode;
        std::string name;
    };

    WifiPhyHelper wifiPhy = WifiPhyHelper::Default();
    YansWifiPhyHelper yansWifiPhy = YansWifiPhyHelper::Default();

    // Use standard DSSS rates (802.11b)
    std::vector<ModeDesc> modes = {
        {WifiPhyHelper::GetDsssRate1Mbps(), "DSSS 1 Mbps"},
        {WifiPhyHelper::GetDsssRate2Mbps(), "DSSS 2 Mbps"},
        {WifiPhyHelper::GetDsssRate5_5Mbps(), "DSSS 5.5 Mbps"},
        {WifiPhyHelper::GetDsssRate11Mbps(), "DSSS 11 Mbps"}
    };

    // Instantiate error rate models
    Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
    Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
    Ptr<WifiErrorRateModel> tableModel = CreateObject<WifiErrorRateModel>();

    // Store Gnuplot datasets for each model and mode
    struct ModelDesc
    {
        std::string name;
        Ptr<ErrorRateModel> model;
        Gnuplot2dDataset datasets[4];
    };
    ModelDesc modelDescs[] = {
        {"YansErrorRateModel", yansModel, {{"Yans 1Mbps"}, {"Yans 2Mbps"}, {"Yans 5.5Mbps"}, {"Yans 11Mbps"}}},
        {"NistErrorRateModel", nistModel, {{"Nist 1Mbps"}, {"Nist 2Mbps"}, {"Nist 5.5Mbps"}, {"Nist 11Mbps"}}},
        {"TableErrorRateModel", tableModel, {{"Table 1Mbps"}, {"Table 2Mbps"}, {"Table 5.5Mbps"}, {"Table 11Mbps"}}}
    };

    // For each model, mode, and SNR, compute frame success rates
    for (uint32_t m = 0; m < sizeof(modelDescs)/sizeof(ModelDesc); ++m)
    {
        for (uint32_t d = 0; d < modes.size(); ++d)
        {
            Ptr<ErrorRateModel> errModel = modelDescs[m].model;
            WifiMode mode = modes[d].mode;
            for (double snr : snrDb)
            {
                double snrLinear = std::pow(10.0, snr/10.0);
                // For DSSS, the SNR parameter is Eb/N0, but NS-3 uses SNR per bit
                double per = errModel->GetChunkSuccessRate(mode, snrLinear, frameSize*8);
                double fsr = per;
                // Validation: Frame success rate must be within [0,1]
                if (fsr < 0.0)
                    fsr = 0.0;
                if (fsr > 1.0)
                    fsr = 1.0;
                modelDescs[m].datasets[d].Add(snr, fsr);
            }
        }
    }

    // Add datasets to gnuplot
    for (ModelDesc &desc : modelDescs)
    {
        for (uint32_t d = 0; d < modes.size(); ++d)
        {
            modelDescs[0].datasets[d].SetStyle(Gnuplot2dDataset::LINES_POINTS);
            modelDescs[1].datasets[d].SetStyle(Gnuplot2dDataset::LINES_POINTS);
            modelDescs[2].datasets[d].SetStyle(Gnuplot2dDataset::LINES_POINTS);
            gnuplot.AddDataset(desc.datasets[d]);
        }
    }

    // Generate plot
    std::ofstream plotFile(outputFileName);
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    Simulator::Destroy();
    return 0;
}