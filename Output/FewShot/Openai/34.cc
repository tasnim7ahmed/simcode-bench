#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

static void WritePlotFile (
    const std::string& filename, 
    const std::string& plotTitle, 
    const std::string& xLabel, 
    const std::string& yLabel, 
    const std::vector<std::string>& seriesNames, 
    const std::vector<std::vector<std::pair<double, double>>>& data)
{
    std::ofstream out (filename);
    out << "set terminal png size 1200,700" << std::endl;
    out << "set output '" << filename << ".png'\n";
    out << "set title \"" << plotTitle << "\"\n";
    out << "set xlabel \"" << xLabel << "\"\n";
    out << "set ylabel \"" << yLabel << "\"\n";
    out << "set key outside\n";
    out << "plot ";
    for (size_t i = 0; i < data.size(); ++i)
    {
        out << "'-' with lines title '" << seriesNames[i] << "'";
        if (i != data.size () - 1) out << ", ";
    }
    out << std::endl;
    for (size_t i = 0; i < data.size(); ++i)
    {
        for (const auto& point : data[i])
        {
            out << std::setprecision(7) << point.first << " " << std::setprecision(10) << point.second << std::endl;
        }
        out << "e" << std::endl;
    }
    out.close();
}

int main (int argc, char *argv[])
{
    uint32_t frameSize = 12000;
    double snrStart = 0.0;
    double snrStop = 40.0;
    double snrStep = 1.0;

    CommandLine cmd;
    cmd.AddValue ("frameSize", "Packet size (bytes)", frameSize);
    cmd.AddValue ("snrStart", "SNR range start (dB)", snrStart);
    cmd.AddValue ("snrStop", "SNR range stop (dB)", snrStop);
    cmd.AddValue ("snrStep", "SNR step (dB)", snrStep);
    cmd.Parse (argc, argv);

    // Get all HE MCS modes
    Ptr<WifiPhy> phy = CreateObject<YansWifiPhy>(); // dummy phy, not used
    WifiHelper wifi;
    WifiStandard standard = WIFI_STANDARD_80211ax;
    wifi.SetStandard (standard);

    std::vector<WifiMode> heModes;
    WifiModeFactory *factory = WifiModeFactory::GetFactory ();
    for (uint8_t mcs = 0; mcs <= 11; ++mcs)
    {
        std::string modeStr = "HeMcs" + std::to_string(mcs) + "80MHz";
        if (factory->Search (modeStr) != WifiModeFactory::m_invalidMode)
        {
            heModes.push_back(factory->Get (modeStr));
        }
    }
    if (heModes.empty())
    {
        // Sometimes above may fail, backup plan for all registered HE modes:
        WifiModeList list = WifiPhy::GetHeMcsList (WIFI_PHY_STANDARD_80211ax_5GHZ);
        heModes.insert (heModes.end(), list.begin(), list.end());
    }
    // Save mode names
    std::vector<std::string> modeNames;
    for (const auto& mode : heModes)
        modeNames.push_back(mode.GetUniqueName());

    // Error rate models
    Ptr<NistErrorRateModel> nist = CreateObject<NistErrorRateModel>();
    Ptr<YansErrorRateModel> yans = CreateObject<YansErrorRateModel>();
    Ptr<WifiErrorRateModel> table = CreateObject<WifiErrorRateModel>();
    std::vector<std::pair<std::string, Ptr<ErrorRateModel>>> models = {
        {"NIST", nist},
        {"YANS", yans},
        {"TABLE", table}
    };

    for (const auto &modelPair : models)
    {
        std::string plotFile = "he_fsr_vs_snr_" + modelPair.first + ".plt";
        std::vector<std::string> seriesNames = modeNames;
        std::vector<std::vector<std::pair<double, double>>> data (heModes.size());

        // For each SNR
        uint32_t idx = 0;
        for (const auto& mode : heModes)
        {
            std::vector<std::pair<double,double>> snr_fsr;
            for (double snrDb = snrStart; snrDb <= snrStop; snrDb += snrStep)
            {
                double snr = std::pow (10.0, snrDb / 10.0);
                double ber = modelPair.second->GetChunkSuccessRate (mode, frameSize * 8, snr); //FSR = CSR
                snr_fsr.emplace_back (snrDb, ber);
            }
            data[idx++] = snr_fsr;
        }
        WritePlotFile(plotFile,
                      "HE FSR vs SNR - " + modelPair.first + " Error Rate Model",
                      "SNR (dB)",
                      "Frame Success Rate",
                      seriesNames, data);
    }

    return 0;
}