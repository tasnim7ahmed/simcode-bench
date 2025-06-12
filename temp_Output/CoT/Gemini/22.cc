#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t frameSize = 1200;
  std::string phyMode("DsssRate1Mbps");

  Gnuplot plot("frame-success-rate-vs-snr.eps");
  plot.SetTerminal("postscript eps color");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.AppendTitle("Frame Success Rate vs. SNR");

  Gnuplot2dDataset yansDataset;
  yansDataset.SetTitle("Yans Error Rate Model");
  yansDataset.SetStyle(Gnuplot2dDataset::LINES);

  Gnuplot2dDataset nistDataset;
  nistDataset.SetTitle("Nist Error Rate Model");
  nistDataset.SetStyle(Gnuplot2dDataset::LINES);

  Gnuplot2dDataset tableDataset;
  tableDataset.SetTitle("Table Error Rate Model");
  tableDataset.SetStyle(Gnuplot2dDataset::LINES);

  std::vector<std::string> dsssModes = {
    "DsssRate1Mbps",
    "DsssRate2Mbps",
    "DsssRate5_5Mbps",
    "DsssRate11Mbps"
  };

  for (const auto& mode : dsssModes) {
    phyMode = mode;

    YansErrorRateModel yansErrorModel;
    NistErrorRateModel nistErrorModel;
    TableErrorRateModel tableErrorModel;

    for (double snr = 0; snr <= 30; snr += 1) {
      double berYans = yansErrorModel.CalculateBer(snr, mode);
      double perYans = 1 - pow((1 - berYans), (8 * frameSize));
      double successRateYans = 1 - perYans;
      yansDataset.Add(snr, successRateYans);

      double berNist = nistErrorModel.CalculateBer(snr, mode);
      double perNist = 1 - pow((1 - berNist), (8 * frameSize));
      double successRateNist = 1 - perNist;
      nistDataset.Add(snr, successRateNist);

      double berTable = tableErrorModel.CalculateBer(snr, mode);
      double perTable = 1 - pow((1 - berTable), (8 * frameSize));
      double successRateTable = 1 - perTable;
      tableDataset.Add(snr, successRateTable);

      NS_ASSERT (successRateYans >= 0.0 && successRateYans <= 1.0);
      NS_ASSERT (successRateNist >= 0.0 && successRateNist <= 1.0);
      NS_ASSERT (successRateTable >= 0.0 && successRateTable <= 1.0);
    }
  }

  plot.AddDataset(yansDataset);
  plot.AddDataset(nistDataset);
  plot.AddDataset(tableDataset);

  std::ofstream plotFile(plot.GetOutputFileName().c_str());
  plot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}