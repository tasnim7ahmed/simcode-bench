/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Error Rate Model Comparison Simulation
 * Compares Nist, Yans, and Table-based error models for 802.11n MCS 0, 4, and 7
 * Plots FER vs. SNR using Gnuplot and outputs an EPS file.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/double.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ErrorModelComparison");

struct ErrorModelConfig {
  std::string name;
  Ptr<WifiPhy> phy;
  Ptr<ErrorRateModel> errorRateModel;
  Gnuplot2dDataset dataset[3]; // One for each MCS (0, 4, 7)
};

double
DbToRatio (double db)
{
  return std::pow (10.0, db / 10.0);
}

double
CalcFer (Ptr<ErrorRateModel> errorRateModel,
         WifiTxVector txVector,
         uint32_t frameSize,
         double snr)
{
  // FER = 1 - (1 - PER)
  double per = errorRateModel->GetChunkSuccessRate (txVector, frameSize, snr);
  return 1.0 - per;
}

// Map MCS index to rates
std::vector<uint8_t> mcsSet = {0, 4, 7};

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1024;
  uint32_t snrStart = 0;
  uint32_t snrStop = 30;
  double   snrStep = 1.0;
  bool useNist = true, useYans = true, useTable = true;

  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame Size in bytes", frameSize);
  cmd.AddValue ("snrStart", "SNR start value (dB)", snrStart);
  cmd.AddValue ("snrStop",  "SNR stop value (dB)", snrStop);
  cmd.AddValue ("snrStep",  "SNR step (dB)", snrStep);
  cmd.AddValue ("useNist",  "Include Nist error model", useNist);
  cmd.AddValue ("useYans",  "Include Yans error model", useYans);
  cmd.AddValue ("useTable", "Include Table-based error model", useTable);
  cmd.Parse (argc, argv);

  WifiPhyStandard standard = WIFI_PHY_STANDARD_80211n_5GHZ;
  std::vector<ErrorModelConfig> models;

  // Prepare the Gnuplot instance
  Gnuplot plot ("error-rate-model-comparison.eps");
  plot.SetTerminal ("postscript eps enhanced color font 'Times,16' size 6,4");
  plot.SetTitle ("FER vs SNR for Nist, Yans, and Table-based Error Rate Models");
  plot.SetLegend ("SNR (dB)", "Frame Error Rate (FER)");

  // Prepare MCS datasets labels
  std::string mcsLabels[] = {"MCS 0", "MCS 4", "MCS 7"};

  if (useNist)
    {
      ErrorModelConfig nistModel;
      nistModel.name = "Nist";
      for (size_t i = 0; i < 3; ++i)
        {
          std::ostringstream oss;
          oss << "Nist (" << mcsLabels[i] << ")";
          nistModel.dataset[i].SetTitle (oss.str());
          nistModel.dataset[i].SetStyle (Gnuplot2dDataset::LINES);
          nistModel.dataset[i].SetErrorBars (Gnuplot2dDataset::NO_ERRORBARS);
          nistModel.dataset[i].SetStyle(Gnuplot2dDataset::LINESPOINTS);
          nistModel.dataset[i].SetPointSize(1.5);
        }
      nistModel.errorRateModel = CreateObject<NistErrorRateModel> ();
      models.push_back (nistModel);
    }
  if (useYans)
    {
      ErrorModelConfig yansModel;
      yansModel.name = "Yans";
      for (size_t i = 0; i < 3; ++i)
        {
          std::ostringstream oss;
          oss << "Yans (" << mcsLabels[i] << ")";
          yansModel.dataset[i].SetTitle (oss.str());
          yansModel.dataset[i].SetStyle (Gnuplot2dDataset::LINES);
          yansModel.dataset[i].SetErrorBars (Gnuplot2dDataset::NO_ERRORBARS);
          yansModel.dataset[i].SetStyle(Gnuplot2dDataset::LINESPOINTS);
          yansModel.dataset[i].SetPointSize(1.5);
        }
      yansModel.errorRateModel = CreateObject<YansErrorRateModel> ();
      models.push_back (yansModel);
    }
  if (useTable)
    {
      ErrorModelConfig tableModel;
      tableModel.name = "Table";
      for (size_t i = 0; i < 3; ++i)
        {
          std::ostringstream oss;
          oss << "Table (" << mcsLabels[i] << ")";
          tableModel.dataset[i].SetTitle (oss.str());
          tableModel.dataset[i].SetStyle (Gnuplot2dDataset::LINES);
          tableModel.dataset[i].SetErrorBars (Gnuplot2dDataset::NO_ERRORBARS);
          tableModel.dataset[i].SetStyle(Gnuplot2dDataset::LINESPOINTS);
          tableModel.dataset[i].SetPointSize(1.5);
        }
      tableModel.errorRateModel = CreateObject<TableBasedErrorRateModel> ();
      models.push_back (tableModel);
    }

  for (double snrDb = snrStart; snrDb <= snrStop; snrDb += snrStep)
    {
      double snrLinear = DbToRatio (snrDb);

      for (size_t mcsIdx = 0; mcsIdx < mcsSet.size(); ++mcsIdx)
        {
          uint8_t mcs = mcsSet[mcsIdx];
          WifiTxVector txVector;
          txVector.SetMode (WifiModeFactory::CreateMcs (standard, mcs));
          txVector.SetPreambleType (WIFI_PREAMBLE_HT_MF);
          txVector.SetNss (1);
          txVector.SetChannelWidth (20);

          for (size_t model = 0; model < models.size (); ++model)
            {
              double fer = CalcFer (models[model].errorRateModel, txVector, frameSize, snrLinear);
              models[model].dataset[mcsIdx].Add (snrDb, fer);
            }
        }
    }

  // Add all datasets to the plot
  for (const auto& model : models)
    {
      for (size_t i = 0; i < 3; ++i)
        {
          plot.AddDataset (model.dataset[i]);
        }
    }

  // Output Gnuplot .plt file and EPS plot
  std::ofstream plotFile ("error-rate-model-comparison.plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();

  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_UNCOND ("Simulation complete. Output written to error-rate-model-comparison.eps");
  return 0;
}