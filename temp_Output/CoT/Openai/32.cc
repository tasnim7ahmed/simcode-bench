#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <map>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HeFrameSuccessRateValidation");

struct ModelSpec
{
  std::string name;
  Ptr<WifiPhy> phy;
  Ptr<ErrorRateModel> errModel;
};

struct GnuplotSet
{
  Gnuplot plot;
  std::vector<Ptr<Gnuplot2dDataset>> datasets; // [mcs]
};

static std::vector<std::string> GetHeMcsStrings ()
{
  // HE supports 0 - 11
  std::vector<std::string> ret;
  for (uint8_t i=0; i<12; ++i)
    {
      ret.push_back ("HE-MCS" + std::to_string (i));
    }
  return ret;
}

int main (int argc, char *argv[])
{
  double snrStart = 0.0;
  double snrEnd = 50.0;
  double snrStep = 1.0;
  uint32_t frameSize = 1500; // bytes
  bool verbose = false;
  std::string outputPrefix = "he-frame-success-rate";
  CommandLine cmd;
  cmd.AddValue ("snrStart", "Start of SNR range", snrStart);
  cmd.AddValue ("snrEnd", "End of SNR range", snrEnd);
  cmd.AddValue ("snrStep", "SNR step", snrStep);
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue ("verbose", "Enable log info", verbose);
  cmd.AddValue ("outputPrefix", "Output prefix for plots", outputPrefix);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnableAll (LOG_PREFIX_FUNC);
      LogComponentEnable ("HeFrameSuccessRateValidation", LOG_LEVEL_INFO);
      LogComponentEnable ("TableBasedErrorRateModel", LOG_LEVEL_INFO);
      LogComponentEnable ("YansErrorRateModel", LOG_LEVEL_INFO);
      LogComponentEnable ("NistErrorRateModel", LOG_LEVEL_INFO);
    }

  std::vector<std::string> heMcs = GetHeMcsStrings ();
  std::vector<ModelSpec> models;
  std::vector<std::string> modelNames { "NIST", "YANS", "Table" };

  uint8_t nMcs = heMcs.size ();
  uint8_t nModels = modelNames.size ();

  WifiPhyHelper phyHelper = WifiPhyHelper::Default ();
  phyHelper.SetChannel (CreateObject<YansWifiChannel> ());
  phyHelper.Set ("Frequency", UintegerValue (5180)); // Use 5 GHz

  WifiHelper wifiHelper; wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211ax);

  // Configure WifiMacType to none: used only for error rate calculation
  Ssid ssid ("test-he");
  WifiMacHelper macHelper;
  macHelper.SetType ("ns3::AdhocWifiMac");

  for (const std::string& modelName : modelNames)
    {
      phyHelper = WifiPhyHelper::Default (); // Reset
      YansWifiPhyHelper yansPhyHelper = YansWifiPhyHelper::Default ();
      phyHelper.SetChannel (yansPhyHelper.CreateChannel ());
      phyHelper.Set ("Frequency", UintegerValue (5180));
      NetDeviceContainer devices;
      NodeContainer c; c.Create (1);

      Ptr<WifiPhy> phy = phyHelper.Create (c.Get (0), macHelper.Create (c.Get (0)));
      Ptr<ErrorRateModel> err;
      if (modelName == "NIST")
        err = CreateObject<NistErrorRateModel> ();
      else if (modelName == "YANS")
        err = CreateObject<YansErrorRateModel> ();
      else if (modelName == "Table")
        err = CreateObject<TableBasedErrorRateModel> ();
      else
        NS_ABORT_MSG ("Unknown model " << modelName);
      phy->SetErrorRateModel (err);
      models.push_back ( {modelName, phy, err} );
    }

  std::map<std::string, GnuplotSet> plots;
  for (const ModelSpec& m : models)
    {
      plots[m.name] = GnuplotSet {
        Gnuplot (outputPrefix + "-" + m.name + ".plt", "Frame Success Rate vs SNR: " + m.name),
        std::vector<Ptr<Gnuplot2dDataset>> ()
      };
      for (uint8_t mcs=0; mcs<nMcs; ++mcs)
        {
          Ptr<Gnuplot2dDataset> ds = CreateObject<Gnuplot2dDataset> ();
          ds->SetTitle (heMcs[mcs]);
          ds->SetStyle (Gnuplot2dDataset::LINES);
          plots[m.name].datasets.push_back (ds);
        }
    }

  // Loop: For each model and each MCS/HE rate
  for (uint8_t midx=0; midx<models.size (); ++midx)
    {
      ModelSpec& model = models[midx];

      for (uint8_t mcs=0; mcs<nMcs; ++mcs)
        {
          WifiTxVector txVector;
          txVector.SetMode (WifiPhy::GetHeMcs (mcs, WIFI_HE_GI_0_8US, WIFI_HE_CHANNEL_WIDTH_80));
          txVector.SetNss (1);
          txVector.SetChannelWidth (80);
          txVector.SetGuardInterval (WIFI_HE_GI_0_8US);
          txVector.SetPreambleType (WIFI_PREAMBLE_HE_SU);

          for (double snr=snrStart; snr<=snrEnd+1e-6; snr+=snrStep)
            {
              double psr = model.phy->GetErrorRateModel ()->CalculatePhyFrameSuccessRate (
                txVector,
                frameSize,
                snr,
                model.phy);
              plots[model.name].datasets[mcs]->Add (snr, psr);
            }
        }
    }

  // Output Gnuplot files
  for (auto& [modelName, gs] : plots)
    {
      for (uint8_t mcs=0; mcs<nMcs; ++mcs)
        {
          gs.plot.AddDataset (*gs.datasets[mcs]);
        }
      gs.plot.SetTitle ("Frame Success Rate vs SNR [" + modelName + " - HE]");
      gs.plot.SetLegend ("SNR [dB]", "FSR");
      std::ofstream f (outputPrefix + "-" + modelName + ".plt");
      gs.plot.GenerateOutput (f);
      f.close ();
    }

  NS_LOG_INFO ("Wrote Gnuplot datasets for all models and HE rates.");

  return 0;
}