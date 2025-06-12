/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2024 ns-3 contributor
 *
 * This program compares the NIST, YANS, and Table-based error rate models in ns-3
 * for EHT (HE/HT) rates. It outputs Frame Success Rate vs SNR for all HT MCS values,
 * using Gnuplot for visualization.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

using namespace ns3;

static const uint32_t FRAME_SIZE = 12000;        // In bits
static const double SNR_START = 0.0;             // dB
static const double SNR_END   = 35.0;            // dB
static const double SNR_STEP  = 1.0;             // dB

class EhtErrorModelEvaluation
{
public:
  void Run ();

private:
  double CalculateSnrBer (Ptr<ErrorRateModel> model,
                          WifiMode mode,
                          uint32_t frameSize,
                          double snrDb);

  void PlotResults (const std::vector<Gnuplot2dDataset>& datasets,
                    const std::string& mcsName);

private:
  std::vector<std::string> modelNames = {"Nist", "Yans", "Table"};
};

double
EhtErrorModelEvaluation::CalculateSnrBer (Ptr<ErrorRateModel> model,
                                          WifiMode mode,
                                          uint32_t frameSize,
                                          double snrDb)
{
  // Convert SNR in dB to linear
  double snr = std::pow (10.0, snrDb / 10.0);
  // Assume noise power = 1, received power = snr
  double ber = model->GetChunkSuccessRate (mode, (size_t)frameSize, snr);
  return ber;
}

void
EhtErrorModelEvaluation::PlotResults (const std::vector<Gnuplot2dDataset>& datasets,
                                      const std::string& mcsName)
{
  Gnuplot plot (mcsName + "_fsr_vs_snr.png");
  plot.SetTitle (mcsName + ": Frame Success Rate vs SNR");
  plot.SetTerminal ("png");
  plot.SetLegend ("SNR (dB)", "Frame Success Rate");

  for (const auto& ds : datasets)
    {
      plot.AddDataset (ds);
    }

  std::ofstream plotFile (mcsName + "_fsr_vs_snr.plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();
}

void
EhtErrorModelEvaluation::Run ()
{
  // Use HT PHY, as EHT-specific error models may be unavailable in ns-3.39+
  WifiPhyStandard phyStandard = WIFI_PHY_STANDARD_80211ax; // EHT: 802.11be, fallback: ax
  WifiHelper wifi;
  wifi.SetStandard (phyStandard);

  // Get all HT MCS modes available
  std::vector<WifiMode> htModes;
  for (uint32_t mcs = 0; mcs < 32; ++mcs)
    {
      std::string mcsStr = "HtMcs" + std::to_string (mcs);
      if (WifiPhy::GetMode (mcsStr).IsValid ())
        {
          htModes.push_back (WifiPhy::GetMode (mcsStr));
        }
    }

  // Prepare various error rate models
  Ptr<ErrorRateModel> nistModel  = CreateObject<NistErrorRateModel> ();
  Ptr<ErrorRateModel> yansModel  = CreateObject<YansErrorRateModel> ();
  Ptr<ErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();

  std::vector<Ptr<ErrorRateModel> > models = {nistModel, yansModel, tableModel};
  std::vector<std::string> modelTitles = {"Nist", "Yans", "Table"};

  // For each HT MCS, plot FSR vs SNR for all three models
  for (const auto& mode : htModes)
    {
      std::string mcsName = mode.GetUniqueName ();
      std::cout << "Processing " << mcsName << std::endl;
      std::vector<Gnuplot2dDataset> datasets (models.size ());

      for (size_t i = 0; i < models.size (); ++i)
        {
          datasets[i].SetTitle (modelTitles[i]);
          datasets[i].SetStyle (Gnuplot2dDataset::LINES_POINTS);

          for (double snrDb = SNR_START; snrDb <= SNR_END; snrDb += SNR_STEP)
            {
              double fsr = CalculateSnrBer (models[i], mode, FRAME_SIZE, snrDb);
              datasets[i].Add (snrDb, fsr);
            }
        }

      PlotResults (datasets, mcsName);
    }
}

// Main
int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  EhtErrorModelEvaluation validator;
  validator.Run ();

  Simulator::Destroy ();
  return 0;
}