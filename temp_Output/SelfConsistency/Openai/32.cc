// he-error-rate-models-validation.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HeErrorRateModelValidation");

// Range of SNR values (dB)
static const double kMinSnrDb = 0.0;
static const double kMaxSnrDb = 40.0;
static const double kSnrStepDb = 1.0;

// Default frame size
static const uint32_t kDefaultFrameSize = 1500;

struct ModelInfo
{
  std::string name;
  Ptr<WifiErrorRateModel> model;
};

int
main (int argc, char *argv[])
{
  uint32_t frameSize = kDefaultFrameSize;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc, argv);

  // Set up WifiHeMcsList for HE rates (MCS 0 - 11 for SU; 0 - 13 for downlink MU)
  WifiPhyStandard standard = WIFI_PHY_STANDARD_80211ax_5GHZ;
  Ptr<YansWifiPhy> phy = CreateObject<YansWifiPhy> ();
  phy->SetStandard (standard);

  // Channel width and guard interval defaults
  uint8_t nss = 1;
  uint8_t mcsMax = 11; // HE supports up to MCS 11 (single-user)
  uint8_t mcsMin = 0;

  // Prepare ErrorRateModel objects
  std::vector<ModelInfo> models;

  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();
  models.push_back ({ "NIST", nistModel });
  models.push_back ({ "YANS", yansModel });
  models.push_back ({ "Table", tableModel });

  // Set up WifiMode for HE rates
  WifiPhyHelper phyHelper = WifiPhyHelper::Default ();
  phyHelper.SetStandard (standard);

  // Use default channel and basic settings for the Phy
  phyHelper.Create (CreateObject<Node> ());

  // Prepare Gnuplot plotting
  // One output file for each model
  std::map<std::string, Gnuplot> plotters;
  for (const auto& modelInfo : models)
    {
      std::ostringstream fn;
      fn << "he-fsr-" << modelInfo.name << ".plt";
      plotters[modelInfo.name] = Gnuplot (fn.str ().c_str ());
      plotters[modelInfo.name].SetTitle (
        "HE Frame Success Rate vs SNR (" + modelInfo.name + " Error Model)");
      plotters[modelInfo.name].SetTerminal ("png");
      plotters[modelInfo.name].SetLegend ("SNR (dB)", "Frame Success Rate");
    }

  // Main processing loop: For each model, for each HE MCS
  for (const auto& modelInfo : models)
    {
      for (uint8_t mcs = mcsMin; mcs <= mcsMax; ++mcs)
        {
          // Use default channel width (e.g., 20MHz)
          WifiMode mode = WifiPhy::GetHeMcs (mcs, nss);

          std::ostringstream label;
          label << "HE-MCS" << unsigned (mcs);

          Gnuplot2dDataset dataset;
          dataset.SetTitle (label.str ());
          dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

          for (double snrDb = kMinSnrDb; snrDb <= kMaxSnrDb; snrDb += kSnrStepDb)
            {
              double snrLinear = std::pow (10.0, snrDb / 10.0);

              // Use BPSK as fallback in case mode is not enabled yet
              if (!mode.IsValid ())
                {
                  continue;
                }

              // Set up necessary parameters for error rate calculation
              // Assume modulation type according to MCS
              double per = modelInfo.model->GetChunkSuccessRate (
                  mode, snrLinear, frameSize * 8);

              double fsr = 1.0 - per;
              dataset.Add (snrDb, fsr);
            }

          plotters[modelInfo.name].AddDataset (dataset);
        }
    }

  // Write Gnuplot output
  for (auto& modelPlot : plotters)
    {
      std::ostringstream datFile;
      datFile << "he-fsr-" << modelPlot.first << ".dat";
      std::ofstream datOutput (datFile.str ());
      modelPlot.second.GenerateOutput (datOutput);
      datOutput.close ();
      // Also print info on console
      std::cout << "Generated: " << datFile.str () << " and Gnuplot script." << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}