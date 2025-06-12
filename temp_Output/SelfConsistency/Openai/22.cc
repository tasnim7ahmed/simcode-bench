/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * DSSS Error Rate Models Validation and Plotting Simulation
 *
 * This simulation calculates and plots the Frame Success Rate against the
 * Signal-to-Noise Ratio (SNR) for different DSSS rates using Yans, Nist,
 * and Table-based error rate models.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  uint32_t frameSize = 1024; // Frame size in bytes

  // Configure Gnuplot output
  std::string fileNameWithNoExtension = "dsss-error-rate-validation";
  std::string plotTitle = "Frame Success Rate vs SNR for DSSS Modes";
  std::string dataTitle = "Frame Success Rate";
  Gnuplot plot (fileNameWithNoExtension + ".eps");
  plot.SetTitle (plotTitle);
  plot.SetTerminal ("postscript eps color enhanced");
  plot.SetLegend ("SNR (dB)", "Frame Success Rate");

  // Define DSSS rates (modes)
  std::vector<std::pair<std::string, WifiMode>> dsssModes = {
    { "1 Mbps", WifiPhy::GetDsssRate1Mbps () },
    { "2 Mbps", WifiPhy::GetDsssRate2Mbps () },
    { "5.5 Mbps", WifiPhy::GetDsssRate5_5Mbps () },
    { "11 Mbps", WifiPhy::GetDsssRate11Mbps () }
  };

  // Prepare SNR points (in dB)
  std::vector<double> snrDbValues;
  for (double snrDb = 0.0; snrDb <= 20.0; snrDb += 0.5)
    {
      snrDbValues.push_back (snrDb);
    }

  // Prepare error rate models
  Ptr<YansErrorRateModel> yansModel = CreateObject<YansErrorRateModel> ();
  Ptr<NistErrorRateModel> nistModel = CreateObject<NistErrorRateModel> ();
  Ptr<TableBasedErrorRateModel> tableModel = CreateObject<TableBasedErrorRateModel> ();

  // Simulation loop for each mode and model
  for (auto &&modePair : dsssModes)
    {
      std::string modeName = modePair.first;
      WifiMode mode = modePair.second;

      // One dataset per error rate model
      Gnuplot2dDataset yansDataset (modeName + " (Yans)");
      Gnuplot2dDataset nistDataset (modeName + " (Nist)");
      Gnuplot2dDataset tableDataset (modeName + " (Table)");

      yansDataset.SetStyle (Gnuplot2dDataset::LINES);
      nistDataset.SetStyle (Gnuplot2dDataset::LINES);
      tableDataset.SetStyle (Gnuplot2dDataset::LINES);

      for (auto &&snrDb : snrDbValues)
        {
          double snrLinear = std::pow (10.0, snrDb / 10.0);

          // Calculate the success probability for each error rate model
          // for the whole frame (frame is 8 * frameSize bits)
          double yansFs = yansModel->GetChunkSuccessRate (mode, snrLinear, frameSize * 8);
          double nistFs = nistModel->GetChunkSuccessRate (mode, snrLinear, frameSize * 8);
          double tableFs = tableModel->GetChunkSuccessRate (mode, snrLinear, frameSize * 8);

          // Ensure value is between 0 and 1
          yansFs = std::min (std::max (yansFs, 0.0), 1.0);
          nistFs = std::min (std::max (nistFs, 0.0), 1.0);
          tableFs = std::min (std::max (tableFs, 0.0), 1.0);

          yansDataset.Add (snrDb, yansFs);
          nistDataset.Add (snrDb, nistFs);
          tableDataset.Add (snrDb, tableFs);
        }

      plot.AddDataset (yansDataset);
      plot.AddDataset (nistDataset);
      plot.AddDataset (tableDataset);
    }

  // Output the plot and inform the user
  std::ofstream plotFile (fileNameWithNoExtension + ".plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();
  std::cout << "Plot generated: " << fileNameWithNoExtension << ".eps" << std::endl;
  std::cout << "Use 'gnuplot " << fileNameWithNoExtension << ".plt' to render output." << std::endl;

  Simulator::Destroy ();
  return 0;
}