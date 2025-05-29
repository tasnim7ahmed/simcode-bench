#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

struct ModelInfo
{
  std::string name;
  Ptr<WifiPhy> phy;
  Ptr<ErrorRateModel> errModel;
};

int main (int argc, char *argv[])
{
  double minSnrDb = 0.0;
  double maxSnrDb = 40.0;
  double snrStepDb = 1.0;
  uint32_t frameSize = 1200;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes (default 1200)", frameSize);
  cmd.AddValue ("minSnrDb", "Minimum SNR dB", minSnrDb);
  cmd.AddValue ("maxSnrDb", "Maximum SNR dB", maxSnrDb);
  cmd.AddValue ("snrStepDb", "SNR step size (dB)", snrStepDb);
  cmd.Parse (argc, argv);

  // List of models to compare
  std::vector<ModelInfo> models;

  // Dummy channel and objects to get access to WifiPhy and ErrorRateModel
  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel> ();
  Ptr<YansWifiPhy> yansPhy = CreateObject<YansWifiPhy> ();
  yansPhy->SetChannel (channel);
  models.push_back ({"Yans", yansPhy, CreateObject<YansErrorRateModel> ()});

  Ptr<YansWifiPhy> nistPhy = CreateObject<YansWifiPhy> ();
  nistPhy->SetChannel (channel);
  models.push_back ({"Nist", nistPhy, CreateObject<NistErrorRateModel> ()});

  Ptr<YansWifiPhy> tablePhy = CreateObject<YansWifiPhy> ();
  tablePhy->SetChannel (channel);
  Ptr<HeTableBasedErrorRateModel> tableModel = CreateObject<HeTableBasedErrorRateModel> ();
  tableModel->LoadTables ();
  models.push_back ({"Table", tablePhy, tableModel});

  // Get all HE rates (MCSs) supported by WifiPhy
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_STANDARD_80211ax);
  WifiMacHelper macHelper;
  Ssid ssid = Ssid ("sim");
  NetDeviceContainer devices;
  NodeContainer nodes;
  nodes.Create (2);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiModeFactory modeFactory (WIFI_STANDARD_80211ax);
  std::vector<WifiMode> heModes;

  for (uint8_t i = 0; i < 12; ++i) // 0-11: HE MCS indices
    {
      auto mode = WifiPhy::GetHeMcs (i, WIFI_HE_GI_0_8US, WIFI_HE_CHANNEL_WIDTH_20MHZ, false, false);
      heModes.push_back (mode);
    }

  // For each model, prepare Gnuplot datasets
  std::vector<Gnuplot2dDataset> datasets [3]; // [model][mcs]
  std::vector<std::string> mcsLabels;
  for (uint32_t m = 0; m < models.size (); ++m)
    {
      for (uint32_t idx = 0; idx < heModes.size (); ++idx)
        {
          std::ostringstream oss;
          oss << "MCS" << static_cast<unsigned> (idx);
          mcsLabels.push_back (oss.str ());
          datasets[m].push_back (Gnuplot2dDataset(oss.str ()));
          datasets[m].back ().SetStyle (Gnuplot2dDataset::LINES);
        }
    }

  // Iterate over SNR values
  for (double snrDb = minSnrDb; snrDb <= maxSnrDb + 1e-6; snrDb += snrStepDb)
    {
      double snrLinear = std::pow (10.0, snrDb / 10.0);

      for (uint32_t m = 0; m < models.size (); ++m)
        {
          // Configure ErrorRateModel in each phy, attach to the phy
          models[m].phy->SetAttribute ("ErrorRateModel", PointerValue (models[m].errModel));

          for (uint32_t idx = 0; idx < heModes.size (); ++idx)
            {
              // Calculate success rate: Psucc = (1 - PER (snr)) for one frame
              double per = models[m].errModel->GetChunkSuccessRate (heModes[idx], snrLinear, frameSize * 8);
              double fsr = per; // ns-3's GetChunkSuccessRate returns success probability for chunk
              datasets[m][idx].Add (snrDb, fsr);
            }
        }
    }

  // Generate Gnuplot outputs
  std::vector<std::string> modelTitles = {"Yans", "Nist", "Table"};
  for (uint32_t m = 0; m < models.size (); ++m)
    {
      Gnuplot plot;
      plot.SetTitle (modelTitles[m] + " Error Model: Frame Success Rate vs SNR, FrameSize=" + std::to_string (frameSize));
      plot.SetLegend ("SNR (dB)", "Frame Success Rate");
      plot.SetExtra ("set xrange [" + std::to_string (minSnrDb) + ":" + std::to_string (maxSnrDb) + "]");
      plot.SetExtra ("set yrange [0:1]");
      for (uint32_t idx = 0; idx < heModes.size (); ++idx)
        {
          plot.AddDataset (datasets[m][idx]);
        }
      std::string filename = "he-fsr-" + modelTitles[m] + "-frame" + std::to_string (frameSize) + ".plt";
      std::ofstream ofs (filename);
      plot.GenerateOutput (ofs);
      ofs.close ();
    }

  Simulator::Destroy ();
  return 0;
}