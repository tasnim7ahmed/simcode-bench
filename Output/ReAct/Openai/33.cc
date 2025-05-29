#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-module.h"
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HtErrorRateModelValidation");

double CalculateFsr(WifiPhy* phy, Ptr<ErrorRateModel> model, WifiMode mode, double snrDb, uint32_t frameSize)
{
  phy->SetErrorRateModel(model);
  phy->SetMode(mode);
  double snrLin = std::pow(10.0, snrDb / 10.0);
  double ber = model->GetChunkSuccessRate(mode, frameSize * 8, snrLin, WifiTxVector());
  // For frames, the frame success rate is just the success probability for the whole frame
  return ber;
}

int main(int argc, char *argv[])
{
  uint32_t frameSize = 1200;
  std::string outputPrefix = "ht-fsr";
  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("outputPrefix", "Gnuplot output prefix", outputPrefix);
  cmd.Parse(argc, argv);

  // Setup Wi-Fi Phy and Channel (dummy, unused in error models, but needed for full API usage)
  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
  Ptr<YansWifiPhy> phy = CreateObject<YansWifiPhy>();
  phy->SetChannel(channel);

  // Create error rate models
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel>();
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel>();
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel>();
  // TableBasedErrorRateModel is loaded with the default table.
  // For validation, stick with defaults.

  struct ModelEntry
  {
    std::string name;
    Ptr<ErrorRateModel> model;
    Gnuplot2dDataset datasets[32];
  };

  // Prepare the models
  std::vector<ModelEntry> models = {
    {"NIST", nistModel},
    {"YANS", yansModel},
    {"TABLE", tableModel}
  };

  // Find all HT modes (MCS0 .. highest available)
  std::vector<WifiMode> htModes;
  WifiHelper wifi;
  WifiPhyHelper phyHelper = WifiPhyHelper::Default ();
  phyHelper.SetChannel ("ns3::YansWifiChannel");
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); // 802.11n/HT
  phyHelper.Set ("ChannelWidth", UintegerValue (20));
  WifiMacHelper mac;
  NetDeviceContainer devs;
  // Use WifiPhy for WifiMode lookup
  WifiModeFactory *factory = WifiModeFactory::GetFactory ();
  uint32_t maxMcs = 0;
  for (uint32_t i = 0;; ++i)
    {
      std::ostringstream os;
      os << "HtMcs" << i;
      std::string modeName = os.str();
      if (WifiModeFactory::Search (modeName) == 0)
        {
          break;
        }
      htModes.push_back(WifiMode(modeName));
      ++maxMcs;
    }

  // SNR sweep range (0 to 40 dB)
  std::vector<double> snrDbVec;
  for (double snr = 0.0; snr <= 40.0; snr += 1.0)
    {
      snrDbVec.push_back(snr);
    }

  // For each model and each MCS, collect FSR points
  for (auto &modelEntry : models)
    {
      for (uint32_t mcs = 0; mcs < htModes.size(); ++mcs)
        {
          std::string modeName = htModes[mcs].GetUniqueName();
          modelEntry.datasets[mcs].SetTitle (modelEntry.name + " " + modeName);
          modelEntry.datasets[mcs].SetStyle (Gnuplot2dDataset::LINES_POINTS);

          for (const auto &snr : snrDbVec)
            {
              double fsr = CalculateFsr(phy, modelEntry.model, htModes[mcs], snr, frameSize);
              modelEntry.datasets[mcs].Add(snr, fsr);
            }
        }
    }

  // Create Gnuplot for each MCS
  for (uint32_t mcs = 0; mcs < htModes.size(); ++mcs)
    {
      std::string fileName = outputPrefix + "-mcs" + std::to_string(mcs) + ".plt";
      std::string plotTitle = "Frame Success Rate vs SNR for HT MCS" + std::to_string(mcs) + " (frameSize=" + std::to_string(frameSize) + ")";
      Gnuplot plot(fileName);
      plot.SetTitle(plotTitle);
      plot.SetLegend("SNR (dB)", "Frame Success Rate");
      plot.SetExtra("set yrange [0:1]");
      plot.SetTerminal("png");

      for (auto &modelEntry : models)
        {
          plot.AddDataset(modelEntry.datasets[mcs]);
        }

      std::ofstream plotFile(fileName);
      plot.GenerateOutput(plotFile);
      plotFile.close();
    }

  std::cout << "All datasets and Gnuplot scripts generated for " << htModes.size() << " HT MCS values.\n";
  return 0;
}