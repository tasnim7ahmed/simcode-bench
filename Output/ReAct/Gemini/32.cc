#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/command-line.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-module.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  uint32_t frameSize = 1200;
  double startSnr = 5.0;
  double stopSnr = 30.0;
  double snrStep = 1.0;

  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.AddValue("startSnr", "Start SNR value in dB", startSnr);
  cmd.AddValue("stopSnr", "Stop SNR value in dB", stopSnr);
  cmd.AddValue("snrStep", "SNR step value in dB", snrStep);
  cmd.Parse(argc, argv);

  std::vector<std::string> errorModels = {"NistErrorRateModel", "YansErrorRateModel", "TableBasedErrorRateModel"};
  std::vector<uint8_t> heMcsValues = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  for (const auto& errorModelName : errorModels) {
    for (uint8_t heMcs : heMcsValues) {
      std::ofstream outputStream(errorModelName + "_HE_MCS_" + std::to_string(int{heMcs}) + ".dat");
      outputStream << "# SNR FrameSuccessRate" << std::endl;

      for (double snr = startSnr; snr <= stopSnr; snr += snrStep) {
        double noiseDbm = -90.0;
        double txPowerDbm = snr + noiseDbm;
        double frameSuccessCount = 0.0;
        uint32_t numTrials = 1000;

        for (uint32_t i = 0; i < numTrials; ++i) {
          Ptr<ErrorRateModel> errorModel;
          if (errorModelName == "NistErrorRateModel") {
            errorModel = CreateObject<NistErrorRateModel>();
          } else if (errorModelName == "YansErrorRateModel") {
            errorModel = CreateObject<YansErrorRateModel>();
          } else {
             errorModel = CreateObject<TableBasedErrorRateModel>();
          }

          WifiMode wifiMode = WifiMode::GetHeMcs(heMcs);
          double symbolTime = wifiMode.GetSymbolTimeUs() * 1e-6;
          double preambleTime = wifiMode.GetPreambleTimeUs() * 1e-6;

          double packetDuration = preambleTime + symbolTime * wifiMode.GetNumberOfSymbols(frameSize);

          double rxPowerW = std::pow(10.0, txPowerDbm / 10.0) * 1e-3;
          double noisePowerW = std::pow(10.0, noiseDbm / 10.0) * 1e-3;
          double sinr = rxPowerW / noisePowerW;
          double signalStrengthDbm = 10 * std::log10(rxPowerW / 1e-3);

          bool success = !errorModel->IsCorrupt(sinr, frameSize * 8, wifiMode);

          if (success) {
            frameSuccessCount += 1.0;
          }
        }

        double frameSuccessRate = frameSuccessCount / numTrials;
        outputStream << snr << " " << frameSuccessRate << std::endl;
      }
      outputStream.close();

      std::ofstream gnuplotStream(errorModelName + "_HE_MCS_" + std::to_string(int{heMcs}) + ".plt");
      gnuplotStream << "set terminal png size 640,480" << std::endl;
      gnuplotStream << "set output '" << errorModelName + "_HE_MCS_" + std::to_string(int{heMcs}) << ".png'" << std::endl;
      gnuplotStream << "set title '" << errorModelName << " - HE MCS " << std::to_string(int{heMcs}) << "'" << std::endl;
      gnuplotStream << "set xlabel 'SNR (dB)'" << std::endl;
      gnuplotStream << "set ylabel 'Frame Success Rate'" << std::endl;
      gnuplotStream << "plot '" << errorModelName + "_HE_MCS_" + std::to_string(int{heMcs}) << ".dat' using 1:2 with lines title 'Frame Success Rate'" << std::endl;
      gnuplotStream.close();
    }
  }

  return 0;
}