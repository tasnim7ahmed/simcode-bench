#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/ssid.h"
#include "ns3/error-model.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t frameSize = 1024;

  Gnuplot plot("dsss_error_rate.eps");
  plot.SetTitle("Frame Success Rate vs. SNR for DSSS Modes");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");

  std::vector<std::string> dsssModes = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};
  std::vector<double> snrValues = {0.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0};

  for (const auto& mode : dsssModes) {
    Gnuplot2dDataset dataset;
    dataset.SetTitle(mode);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (double snr : snrValues) {
      // Yans Error Rate Model
      Ptr<YansErrorRateModel> yansErrorRateModel = CreateObject<YansErrorRateModel>();
      yansErrorRateModel->SetAttribute("Ber", DoubleValue(ErrorModel::ConvertBerToBr(1e-5)));

      // NIST Error Rate Model
      Ptr<NistErrorRateModel> nistErrorRateModel = CreateObject<NistErrorRateModel>();
      nistErrorRateModel->SetAttribute("Ber", DoubleValue(ErrorModel::ConvertBerToBr(1e-5)));

      // Table Error Rate Model (Example with dummy data)
      Ptr<RateErrorModel> tableErrorRateModel = CreateObject<RateErrorModel>();
      tableErrorRateModel->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
      tableErrorRateModel->SetAttribute("ErrorRate", DoubleValue(1e-5));

      // Calculate Frame Success Rate (Placeholder - Actual calculation depends on the model)
      double frameSuccessRateYans = 1.0 - (yansErrorRateModel->GetErrorRate(snr, mode, frameSize) / (double)frameSize);
      double frameSuccessRateNist = 1.0 - (nistErrorRateModel->GetErrorRate(snr, mode, frameSize) / (double)frameSize);
      double frameSuccessRateTable = 1.0 - (tableErrorRateModel->GetErrorRate(snr, mode, frameSize) / (double)frameSize);

      // Average the results (or use a specific model)
      double frameSuccessRate = (frameSuccessRateYans + frameSuccessRateNist + frameSuccessRateTable) / 3.0;

      dataset.Add(snr, frameSuccessRate);

      // Validate the result (Example: Frame Success Rate should be between 0 and 1)
      if (frameSuccessRate < 0.0 || frameSuccessRate > 1.0) {
        std::cerr << "Error: Frame Success Rate out of range for mode " << mode << " and SNR " << snr << std::endl;
      }
    }
    plot.AddDataset(dataset);
  }
  plot.GenerateOutput();

  return 0;
}