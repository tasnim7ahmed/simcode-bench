/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * ns-3 HT Error Rate Model Validation with NIST, YANS, and Table-based models.
 * Generates Gnuplot plots: Frame Success Rate vs. SNR for each model and HT MCS.
 */

#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HtErrorRateValidation");

void
GeneratePlot(const std::string& modelName,
             Ptr<ErrorRateModel> model,
             const std::vector<WifiMode>& htModes,
             double minSnrDb,
             double maxSnrDb,
             double stepSnrDb,
             uint32_t frameSize,
             const std::string& outputFileName)
{
  Gnuplot plot (outputFileName);
  plot.SetTitle ("Frame Success Rate vs SNR - " + modelName + " Error Model");
  plot.SetTerminal ("png");
  plot.SetLegend ("SNR (dB)", "Frame Success Rate");

  for (const auto& mode : htModes)
    {
      std::ostringstream label;
      label << "MCS" << std::setw(2) << std::setfill('0') << mode.GetMcsValue();
      Gnuplot2dDataset dataset (label.str());
      dataset.SetStyle (Gnuplot2dDataset::LINES);

      for (double snrDb = minSnrDb; snrDb <= maxSnrDb; snrDb += stepSnrDb)
        {
          double snrLinear = std::pow(10.0, snrDb / 10.0);
          double frameErrorRate = model->GetChunkSuccessRate (mode, snrLinear, frameSize);
          double frameSuccessRate = frameErrorRate; // Actually, it's *success* rate.
          dataset.Add (snrDb, frameSuccessRate);
        }
      plot.AddDataset (dataset);
    }

  std::ofstream plotFile (outputFileName.c_str());
  plot.GenerateOutput (plotFile);
  plotFile.close ();
}

std::vector<WifiMode>
GetHtModes ()
{
  std::vector<WifiMode> htModes;
  WifiPhyHelper phyHelper = WifiPhyHelper::Default ();
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n);
  std::vector<std::string> mcsModeStrings = {
      "HtMcs0", "HtMcs1", "HtMcs2", "HtMcs3", "HtMcs4", "HtMcs5", "HtMcs6", "HtMcs7",
      "HtMcs8", "HtMcs9", "HtMcs10", "HtMcs11", "HtMcs12", "HtMcs13", "HtMcs14", "HtMcs15"
  };

  for (const std::string& mcsStr : mcsModeStrings)
    {
      if (WifiPhy::IsModeSupported (WIFI_PHY_STANDARD_80211n, mcsStr.c_str()))
        {
          htModes.push_back (WifiPhy::GetMode (WIFI_PHY_STANDARD_80211n, mcsStr.c_str()));
        }
    }
  return htModes;
}

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1500;
  double minSnrDb = 0.0;
  double maxSnrDb = 30.0;
  double stepSnrDb = 1.0;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size (bytes)", frameSize);
  cmd.AddValue ("minSnrDb",   "Minimum SNR in dB", minSnrDb);
  cmd.AddValue ("maxSnrDb",   "Maximum SNR in dB", maxSnrDb);
  cmd.AddValue ("stepSnrDb",  "Step SNR in dB",    stepSnrDb);
  cmd.Parse (argc, argv);

  // Obtain all supported HT modes.
  std::vector<WifiMode> htModes = GetHtModes ();
  if (htModes.empty ())
    {
      NS_LOG_ERROR ("No HT MCS modes available!");
      return 1;
    }

  // NIST Error Rate Model
  NS_LOG_UNCOND ("Generating NIST error rate curve...");
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  GeneratePlot ("NIST", nistModel, htModes, minSnrDb, maxSnrDb, stepSnrDb, frameSize,
                "ht-nist-frame-success-rate.png");

  // YANS Error Rate Model
  NS_LOG_UNCOND ("Generating YANS error rate curve...");
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  GeneratePlot ("YANS", yansModel, htModes, minSnrDb, maxSnrDb, stepSnrDb, frameSize,
                "ht-yans-frame-success-rate.png");

  // Table-based Error Rate Model
  NS_LOG_UNCOND ("Generating Table-based error rate curve...");
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();
  GeneratePlot ("Table-Based", tableModel, htModes, minSnrDb, maxSnrDb, stepSnrDb, frameSize,
                "ht-table-frame-success-rate.png");

  NS_LOG_UNCOND ("Plot generation complete.");
  return 0;
}