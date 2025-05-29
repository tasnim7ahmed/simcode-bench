#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HtFrameSuccessVsSnr");

static std::vector<Ptr<ErrorRateModel>> GetErrorRateModels ()
{
  std::vector<Ptr<ErrorRateModel>> models;
  models.push_back (CreateObject<NistErrorRateModel> ());
  models.push_back (CreateObject<YansErrorRateModel> ());
  models.push_back (CreateObject<TableBasedErrorRateModel> ());
  return models;
}

static std::vector<std::string> GetErrorRateModelNames ()
{
  std::vector<std::string> names;
  names.push_back ("Nist");
  names.push_back ("Yans");
  names.push_back ("Table");
  return names;
}

static std::string GetHtMcsName (WifiMode mode)
{
  std::ostringstream oss;
  if (mode.GetModulationClass () == WIFI_MOD_CLASS_HT)
    {
      oss << "MCS" << static_cast<int>(mode.GetMcsValue ());
      return oss.str ();
    }
  else
    {
      return "Unknown";
    }
}

int main (int argc, char *argv[])
{
  double minSnr = 0.0; // dB
  double maxSnr = 40.0; // dB
  double stepSnr = 1.0; // dB
  uint32_t frameSize = 1200; // bytes
  std::string outputPrefix = "ht-fsr-vs-snr";
  CommandLine cmd;
  cmd.AddValue ("minSnr", "Minimum SNR in dB", minSnr);
  cmd.AddValue ("maxSnr", "Maximum SNR in dB", maxSnr);
  cmd.AddValue ("stepSnr", "SNR step in dB", stepSnr);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue ("outputPrefix", "Output file prefix", outputPrefix);
  cmd.Parse (argc, argv);

  // Prepare error rate models and their names
  std::vector<Ptr<ErrorRateModel>> errModels = GetErrorRateModels ();
  std::vector<std::string> errModelNames = GetErrorRateModelNames ();

  // Find all HT MCS rates from a WifiPhyStandard that supports HT
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);
  Ptr<WifiPhy> phy = CreateObject<YansWifiPhy> ();
  WifiPhyHelper phyHelper = WifiPhyHelper::Default ();
  phyHelper.SetStandard (WIFI_STANDARD_80211n_5GHZ);
  std::vector<WifiMode> modes;

  for (uint32_t i = 0; ; ++i)
    {
      WifiMode mode = phy->GetMode (i);
      if (mode.IsValid () && mode.GetModulationClass () == WIFI_MOD_CLASS_HT)
        {
          modes.push_back (mode);
        }
      else if (!mode.IsValid ())
        {
          break;
        }
    }

  // Remove duplicate MCSs (some configurations may provide MCS0..MCS7, MCS8..MCS15, etc)
  std::vector<WifiMode> uniqueModes;
  std::set<uint8_t> mcsSeen;
  for (const auto &mode : modes)
    {
      uint8_t mcs = mode.GetMcsValue ();
      if (mcsSeen.find (mcs) == mcsSeen.end ())
        {
          uniqueModes.push_back (mode);
          mcsSeen.insert (mcs);
        }
    }
  modes = uniqueModes;

  // Set up Gnuplot files
  // For each error model, one Gnuplot file with FSR vs SNR for each HT MCS
  std::vector<std::unique_ptr<Gnuplot>> plots;
  std::vector<std::vector<std::unique_ptr<Gnuplot2dDataset>>> datasets;
  for (size_t m = 0; m < errModels.size (); ++m)
    {
      std::string gnuplotTitle = "Frame Success Rate vs SNR (" + errModelNames[m] + ")";
      plots.emplace_back (std::make_unique<Gnuplot> (outputPrefix + "-" + errModelNames[m] + ".plt", gnuplotTitle));
      plots[m]->SetTerminal ("png");
      plots[m]->SetLegend ("SNR (dB)", "Frame Success Rate");
      std::vector<std::unique_ptr<Gnuplot2dDataset>> modelDatasets;
      for (size_t j = 0; j < modes.size (); ++j)
        {
          std::string mcsName = GetHtMcsName (modes[j]);
          modelDatasets.emplace_back (std::make_unique<Gnuplot2dDataset> (mcsName));
          modelDatasets[j]->SetStyle (Gnuplot2dDataset::LINES);
        }
      datasets.push_back (std::move (modelDatasets));
    }

  // Main: for all error rate models, for all SNRs, for all HT MCS, compute FSR and save in datasets
  for (double snrDb = minSnr; snrDb <= maxSnr + 1e-6; snrDb += stepSnr)
    {
      double snrLinear = std::pow (10.0, snrDb / 10.0);

      for (size_t m = 0; m < errModels.size (); ++m)
        {
          Ptr<ErrorRateModel> err = errModels[m];
          for (size_t j = 0; j < modes.size (); ++j)
            {
              WifiMode mode = modes[j];
              uint32_t size = frameSize;
              double per = err->GetChunkSuccessRate (mode, snrLinear, size, WifiTxVector ());
              double fsr = per; // Actually, GetChunkSuccessRate returns *success* probability [0,1]
              datasets[m][j]->Add (snrDb, fsr);
            }
        }
    }

  // Write out Gnuplot datasets
  for (size_t m = 0; m < errModels.size (); ++m)
    {
      for (size_t j = 0; j < modes.size (); ++j)
        {
          plots[m]->AddDataset (*(datasets[m][j]));
        }
      std::ofstream plotFile ((outputPrefix + "-" + errModelNames[m] + ".plt").c_str ());
      plots[m]->GenerateOutput (plotFile);
      plotFile.close ();
    }
  return 0;
}