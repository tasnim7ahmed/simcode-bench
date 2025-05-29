#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-standards.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t frameSize = 12000;

  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse(argc, argv);

  std::vector<std::string> errorModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<std::string> channelWidths = {"20MHz"};
  std::vector<uint8_t> mcsValues;

  for (uint8_t i = 0; i <= 9; ++i) {
      mcsValues.push_back(i);
  }
  mcsValues.erase(std::remove(mcsValues.begin(), mcsValues.end(), 9), mcsValues.end());

  for (const auto& channelWidth : channelWidths) {
      for (const auto& errorModelName : errorModels) {
          for (uint8_t mcs : mcsValues) {
            std::string dataNamePrefix = "vht_" + channelWidth + "_" + errorModelName + "_mcs" + std::to_string(mcs);

              Gnuplot plot;
              plot.SetTitle(dataNamePrefix);
              plot.SetLegend("SNR (dB)", "Frame Success Rate");

              Gnuplot2dDataset dataset;
              dataset.SetTitle(dataNamePrefix);
              dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

              for (int snr = -5; snr <= 30; ++snr) {
                  double ber = 0.0;
                  double per = 0.0;

                  if (errorModelName == "NistErrorRateModel") {
                      NistErrorRateModel errorModel;
                      WifiMode mode = WifiMode::GetVhtMode(VhtWifiBandwidth::VHT_BW_20, mcs);
                      ber = errorModel.CalculateBer(snr, mode);
                      per = 1 - pow((1 - ber), (8 * frameSize));
                  } else if (errorModelName == "YansErrorRateModel") {
                      YansErrorRateModel errorModel;
                      WifiMode mode = WifiMode::GetVhtMode(VhtWifiBandwidth::VHT_BW_20, mcs);
                      ber = errorModel.CalculateBer(snr, mode);
                      per = 1 - pow((1 - ber), (8 * frameSize));
                  } else if (errorModelName == "TableBasedErrorRateModel") {
                      TableBasedErrorRateModel errorModel;
                      WifiMode mode = WifiMode::GetVhtMode(VhtWifiBandwidth::VHT_BW_20, mcs);
                      ber = errorModel.CalculateBer(snr, mode);
                      per = 1 - pow((1 - ber), (8 * frameSize));
                  }

                  dataset.Add(snr, 1 - per);
              }

              plot.AddDataset(dataset);
              plot.GenerateOutput(dataNamePrefix + ".png");
              plot.GenerateOutput(dataNamePrefix + ".plt");

          }
      }
  }
  return 0;
}