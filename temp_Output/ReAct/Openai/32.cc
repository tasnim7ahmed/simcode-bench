#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>

using namespace ns3;

// Parameters
static const uint32_t kFrameSize = 1200; // bytes, can be changed via command line
static const double kSnrStart = 0.0;
static const double kSnrEnd = 40.0;
static const double kSnrStep = 1.0;
static const uint8_t kMinHeMcs = 0;
static const uint8_t kMaxHeMcs = 11; // HE supports MCS 0-11

int main(int argc, char *argv[])
{
    uint32_t frameSize = kFrameSize;
    CommandLine cmd;
    cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
    cmd.Parse(argc, argv);

    // Set up WifiPhy (just for reference, we use only helpers for validation)
    Ptr<WifiPhy> phy;
    WifiPhyHelper phyHelper = WifiPhyHelper::Default();
    phy = phyHelper.Create<YansWifiPhy>();

    std::vector<std::string> errorModels = {"nist", "yans", "table"};
    std::vector<Ptr<WifiPhy>> phyVec;

    // Construct one error model of each type, with HE capabilities
    // NIST
    Ptr<YansWifiPhy> nistPhy = CreateObject<YansWifiPhy>();
    Ptr<NistErrorRateModel> nistError = CreateObject<NistErrorRateModel>();
    nistPhy->SetErrorRateModel(nistError);
    WifiPhyAttributesHelper::SetDefault("Standard", StringValue("802.11ax"));
    phyVec.push_back(nistPhy);
    // YANS
    Ptr<YansWifiPhy> yansPhy = CreateObject<YansWifiPhy>();
    Ptr<YansErrorRateModel> yansError = CreateObject<YansErrorRateModel>();
    yansPhy->SetErrorRateModel(yansError);
    phyVec.push_back(yansPhy);
    // Table
    Ptr<YansWifiPhy> tablePhy = CreateObject<YansWifiPhy>();
    Ptr<TableBasedErrorRateModel> tableError = CreateObject<TableBasedErrorRateModel>();
    tableError->LoadDefaultTables(); // Ensure tables are loaded
    tablePhy->SetErrorRateModel(tableError);
    phyVec.push_back(tablePhy);

    // Prepare Gnuplot
    Gnuplot plot("he_fsr_vs_snr.png");
    plot.SetTitle("Frame Success Rate vs SNR for HE MCS (frame size = " + std::to_string(frameSize) + " bytes)");
    plot.SetTerminal("png");
    plot.SetLegend("SNR (dB)", "Frame Success Rate");

    std::vector<Gnuplot2dDataset> datasets;

    for (uint32_t modelIdx = 0; modelIdx < errorModels.size(); ++modelIdx)
    {
        std::string model = errorModels[modelIdx];
        Ptr<WifiPhy> wphy = phyVec[modelIdx];

        for (uint8_t mcs = kMinHeMcs; mcs <= kMaxHeMcs; ++mcs)
        {
            // Build dataset key
            std::ostringstream label;
            label << model << " MCS=" << int(mcs);
            Gnuplot2dDataset dataset;
            dataset.SetTitle(label.str());
            dataset.SetStyle(Gnuplot2dDataset::LINES);

            // Create Wifi modes (HE MCS)
            WifiPhyStandard standard = WIFI_PHY_STANDARD_80211ax;
            HeSupportedMcsAndNss heMcsNss;
            heMcsNss.rxMcsBitmask.push_back((1 << (mcs+1)) - 1); // support up to this MCS
            heMcsNss.txMcsBitmask.push_back((1 << (mcs+1)) - 1);
            Ssid ssid = Ssid("he-validation");
            WifiMacHelper macHelper;
            macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
            WifiHelper wifiHelper;
            wifiHelper.SetStandard(standard);

            WifiPhyHelper::Default();
            std::vector<WifiMode> heModes = WifiPhy::GetHeMcs0ToHeMcs11();

            if (mcs >= heModes.size()) // Defensive: should not fire for 0..11
                continue;
 
            WifiMode heMode = heModes[mcs];

            // For each SNR, compute FSR
            for (double snrDb = kSnrStart; snrDb <= kSnrEnd; snrDb += kSnrStep)
            {
                double snr = std::pow(10.0, snrDb / 10.0);

                // Get error rate model for this PHY
                Ptr<ErrorRateModel> errorModel = wphy->GetErrorRateModel();

                // Compute frame error rate
                double fer = 1.0;
                // Need required parameters
                if (model == "table")
                {
                    // TableBased model only valid for known modes in table
                    if (DynamicCast<TableBasedErrorRateModel>(errorModel)->IsModeSupported(heMode))
                    {
                        fer = errorModel->GetChunkSuccessRate(heMode, snr, frameSize * 8);
                    }
                    else
                    {
                        fer = 1.0; // unsupported, so FSR=0
                    }
                }
                else
                {
                    fer = errorModel->GetChunkSuccessRate(heMode, snr, frameSize * 8);
                }
                double fsr = fer;
                dataset.Add(snrDb, fsr);
            }
            datasets.push_back(dataset);
        }
    }

    // Add all datasets to plot
    for (auto& dataset : datasets)
    {
        plot.AddDataset(dataset);
    }

    // Output Gnuplot data to file
    std::ofstream plotFile("he_fsr_vs_snr.plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();

    std::cout << "Plot saved to 'he_fsr_vs_snr.plt'. Convert to .png with gnuplot." << std::endl;

    return 0;
}