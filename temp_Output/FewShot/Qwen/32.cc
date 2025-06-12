#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeErrorRateModelComparison");

double
CalculateFsrForMcsAndSnr(uint8_t mcsValue, double snrDb, uint32_t frameSize, WifiModulationClass modulationClass)
{
    Ptr<WifiPhy> phy = CreateObject<WifiPhy>();
    phy->SetErrorRateModel(WifiHelper::DefaultErrorRateModel());
    phy->SetDevice(CreateObject<WifiNetDevice>());
    phy->ConfigureStandard(WIFI_STANDARD_80211ax);

    WifiMode mode;
    if (modulationClass == WIFI_MOD_CLASS_HE)
    {
        mode = WifiModeFactory::CreateWifiMode("HeMcs" + std::to_string(mcsValue));
    }
    else
    {
        NS_FATAL_ERROR("Unsupported modulation class");
    }

    // Create a packet of the specified size
    Ptr<Packet> pkt = Create<Packet>(frameSize);
    WifiTxVector txVector(mode, 0, WIFI_PREAMBLE_HE_SU, 800, 1, 1, 0, 20, false);

    // Convert SNR from dB to linear
    double snrLinear = std::pow(10.0, snrDb / 10.0);

    // Calculate success rate using PhyEntity's method
    return phy->GetPhyEntity(modulationClass)->GetFrameSuccessRate(pkt, txVector, snrLinear);
}

int
main(int argc, char* argv[])
{
    uint32_t frameSize = 1472; // Default frame size in bytes
    std::string outputPlotFileName("he-fsr-vs-snr");

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "The frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    GnuplotCollection gnuplots(outputPlotFileName + ".png");
    gnuplots.SetTitle("Frame Success Rate vs SNR for HE MCS values");
    gnuplots.SetTerminal("png");
    gnuplots.SetLegend("SNR (dB)", "Frame Success Rate");

    for (uint8_t errorModelIndex = 0; errorModelIndex < 3; ++errorModelIndex)
    {
        std::string errorModelName;
        switch (errorModelIndex)
        {
        case 0:
            Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("NistErrorRateModel"));
            errorModelName = "NIST";
            break;
        case 1:
            Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("YansErrorRateModel"));
            errorModelName = "YANS";
            break;
        case 2:
            Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue("TableBasedErrorRateModel"));
            errorModelName = "Table-based";
            break;
        default:
            errorModelName = "Unknown";
        }

        Gnuplot2dDataset dataset(errorModelName);
        dataset.style = Gnuplot2dDataset::LINES_POINTS;

        for (double snrDb = 0; snrDb <= 30; snrDb += 2)
        {
            double averageFsr = 0.0;
            for (uint8_t mcsValue = 0; mcsValue <= 11; ++mcsValue) // HE MCS 0-11
            {
                double fsr = CalculateFsrForMcsAndSnr(mcsValue, snrDb, frameSize, WIFI_MOD_CLASS_HE);
                averageFsr += fsr;
            }
            averageFsr /= 12.0;
            dataset.Add(snrDb, averageFsr);
        }

        gnuplots.AddDataset(dataset);
    }

    {
        std::ofstream plotFile(outputPlotFileName + ".plt");
        gnuplots.GenerateOutput(plotFile);
    }

    return 0;
}