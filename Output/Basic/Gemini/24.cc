#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableNist = true;
  bool enableYans = true;
  bool enableTable = true;
  uint32_t frameSize = 1000;
  uint32_t minMcs = 0;
  uint32_t maxMcs = 7;
  double snrStep = 0.5;

  CommandLine cmd;
  cmd.AddValue("enableNist", "Enable Nist error model", enableNist);
  cmd.AddValue("enableYans", "Enable Yans error model", enableYans);
  cmd.AddValue("enableTable", "Enable Table error model", enableTable);
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("minMcs", "Minimum MCS value", minMcs);
  cmd.AddValue("maxMcs", "Maximum MCS value", maxMcs);
  cmd.AddValue("snrStep", "SNR step value", snrStep);
  cmd.Parse(argc, argv);

  LogComponentEnable("ErrorRateComparison", LOG_LEVEL_INFO);

  Gnuplot2dDataset datasetNist;
  datasetNist.SetTitle("Nist Error Model");
  datasetNist.SetStyle(Gnuplot2dDataset::LINES);

  Gnuplot2dDataset datasetYans;
  datasetYans.SetTitle("Yans Error Model");
  datasetYans.SetStyle(Gnuplot2dDataset::LINES);

  Gnuplot2dDataset datasetTable;
  datasetTable.SetTitle("Table Error Model");
  datasetTable.SetStyle(Gnuplot2dDataset::LINES);

  for (uint32_t mcs = minMcs; mcs <= maxMcs; ++mcs) {
    if (mcs != 0 && mcs != 4 && mcs != 7) continue;

    std::vector<std::pair<double, double>> ferNist;
    std::vector<std::pair<double, double>> ferYans;
    std::vector<std::pair<double, double>> ferTable;

    for (double snr = 0; snr <= 30; snr += snrStep) {
      double berNist = 0.0;
      double berYans = 0.0;
      double berTable = 0.0;

      if (enableNist) {
        NistErrorRateModel errorModelNist;
        berNist = errorModelNist.GetBitErrorRate(snr, mcs);
      }
      if (enableYans) {
        YansErrorRateModel errorModelYans;
        berYans = errorModelYans.GetBitErrorRate(snr, mcs);
      }
      if (enableTable) {
        TableErrorRateModel errorModelTable;
        berTable = errorModelTable.GetBitErrorRate(snr, mcs);
      }

      double ferNistValue = 1 - std::pow((1 - berNist), (frameSize * 8));
      double ferYansValue = 1 - std::pow((1 - berYans), (frameSize * 8));
      double ferTableValue = 1 - std::pow((1 - berTable), (frameSize * 8));

      if (enableNist) {
        ferNist.push_back(std::make_pair(snr, ferNistValue));
      }
      if (enableYans) {
        ferYans.push_back(std::make_pair(snr, ferYansValue));
      }
      if (enableTable) {
        ferTable.push_back(std::make_pair(snr, ferTableValue));
      }
    }

    std::string mcsLabel = "MCS " + std::to_string(mcs);
    if (enableNist) {
      datasetNist.Add(ferNist);
      datasetNist.SetExtraTitle(mcsLabel);
    }
    if (enableYans) {
      datasetYans.Add(ferYans);
      datasetYans.SetExtraTitle(mcsLabel);
    }
    if (enableTable) {
      datasetTable.Add(ferTable);
      datasetTable.SetExtraTitle(mcsLabel);
    }
  }

  Gnuplot plot("error_rate_comparison.eps");
  plot.SetTitle("FER vs SNR");
  plot.SetTerminal("postscript eps color");
  plot.SetLegend("SNR (dB)", "FER");
  if (enableNist) plot.AddDataset(datasetNist);
  if (enableYans) plot.AddDataset(datasetYans);
  if (enableTable) plot.AddDataset(datasetTable);
  plot.GenerateOutput();

  return 0;
}