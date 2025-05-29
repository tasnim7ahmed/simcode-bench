#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/command-line.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t frameSize = 12000;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log iframes to stdout", verbose);
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse(argc, argv);

  std::vector<double> snrValues;
  for (double snr = 5.0; snr <= 30.0; snr += 1.0) {
    snrValues.push_back(snr);
  }

  std::vector<WifiMode> heModes;
  heModes.push_back(WifiMode("HE_MCS0"));
  heModes.push_back(WifiMode("HE_MCS1"));
  heModes.push_back(WifiMode("HE_MCS2"));
  heModes.push_back(WifiMode("HE_MCS3"));
  heModes.push_back(WifiMode("HE_MCS4"));
  heModes.push_back(WifiMode("HE_MCS5"));
  heModes.push_back(WifiMode("HE_MCS6"));
  heModes.push_back(WifiMode("HE_MCS7"));
  heModes.push_back(WifiMode("HE_MCS8"));
  heModes.push_back(WifiMode("HE_MCS9"));
  heModes.push_back(WifiMode("HE_MCS10"));
  heModes.push_back(WifiMode("HE_MCS11"));

  std::vector<std::string> errorRateModelNames = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};

  for (const auto& modelName : errorRateModelNames) {
    for (const auto& mode : heModes) {
      std::string filename = modelName + "_" + mode.GetName() + ".dat";
      std::ofstream output(filename);
      output << "# SNR FrameSuccessRate" << std::endl;

      for (double snr : snrValues) {
        Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(modelName));
        if (modelName == "TableBasedErrorRateModel"){
            Config::SetDefault("ns3::TableBasedErrorRateModel::FileName", StringValue("contrib/wifi/model/he-ber.dat"));
        }

        double ber;

        if (modelName == "NistErrorRateModel") {
            ber = NistErrorRateModel::BerFromCnr(snr, mode);
        } else if (modelName == "YansErrorRateModel"){
            ber = YansErrorRateModel::BerFromCnr(snr, mode);
        } else {
            double noisePowerW = 1e-10;
            double signalPowerW = noisePowerW * pow(10.0, snr/10.0);
            double signalPowerDbm = 10 * log10(signalPowerW / 0.001);
            Ptr<ErrorRateModel> errorModel = CreateObject<TableBasedErrorRateModel>();
            errorModel->SetAttribute ("FileName", StringValue("contrib/wifi/model/he-ber.dat"));
            ber = errorModel->CalculateBitErrorRate(signalPowerDbm, mode);
        }

        double packetErrorRate = 1 - pow(1 - ber, frameSize * 8);
        double frameSuccessRate = 1 - packetErrorRate;

        output << snr << " " << frameSuccessRate << std::endl;
      }
      output.close();
    }
  }

  Gnuplot gnuplot;
  gnuplot.SetTitle("Frame Success Rate vs. SNR");
  gnuplot.SetLegend("Error Model, MCS");
  gnuplot.SetXAxis("SNR (dB)");
  gnuplot.SetYAxis("Frame Success Rate");

  for (const auto& modelName : errorRateModelNames) {
    for (const auto& mode : heModes) {
      std::string filename = modelName + "_" + mode.GetName() + ".dat";
      gnuplot.AddDataset(filename, modelName + ", " + mode.GetName());
    }
  }

  gnuplot.GenerateOutput("frame-success-rate.plt");

  return 0;
}