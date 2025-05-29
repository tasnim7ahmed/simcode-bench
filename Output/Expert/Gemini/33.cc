#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-error-model.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  uint32_t frameSize = 1000;
  cmd.AddValue("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse(argc, argv);

  std::vector<double> snrValues = {5, 10, 15, 20, 25, 30};
  std::vector<WifiMode> htModes = {
      WifiMode("HtMcs0"), WifiMode("HtMcs1"),  WifiMode("HtMcs2"),
      WifiMode("HtMcs3"), WifiMode("HtMcs4"),  WifiMode("HtMcs5"),
      WifiMode("HtMcs6"), WifiMode("HtMcs7"),  WifiMode("HtMcs8"),
      WifiMode("HtMcs9"), WifiMode("HtMcs10"), WifiMode("HtMcs11"),
      WifiMode("HtMcs12"), WifiMode("HtMcs13"), WifiMode("HtMcs14"),
      WifiMode("HtMcs15")};

  std::vector<std::string> modelNames = {"Nist", "Yans", "Table"};
  std::map<std::string, std::string> modelFileNames = {
      {"Nist", "nist-ht.dat"},
      {"Yans", "yans-ht.dat"},
      {"Table", "table-ht.dat"}};
  
  std::map<std::string, ErrorRateModel *> errorRateModels;
  errorRateModels["Nist"] = new NistErrorRateModel();
  errorRateModels["Yans"] = new YansErrorRateModel();
  errorRateModels["Table"] = new TableErrorRateModel();

  for (const auto& modelName : modelNames) {
      std::string fileName = modelFileNames[modelName];
      if (modelName == "Table") {
          Config::SetDefault("ns3::TableErrorRateModel::FileName", StringValue(fileName));
      }
  }

  for (const auto& modelName : modelNames) {
    std::ofstream outFile;
    outFile.open(modelName + ".plt");
    outFile << "set terminal png size 1024,768" << std::endl;
    outFile << "set output '" << modelName << ".png'" << std::endl;
    outFile << "set title '" << modelName << " Frame Success Rate vs SNR'" << std::endl;
    outFile << "set xlabel 'SNR (dB)'" << std::endl;
    outFile << "set ylabel 'Frame Success Rate'" << std::endl;
    outFile << "set grid" << std::endl;
    outFile << "plot ";

    for (size_t i = 0; i < htModes.size(); ++i) {
      std::ofstream dataFile;
      std::string dataFileName = modelName + "_mcs" + std::to_string(i) + ".dat";
      dataFile.open(dataFileName);

      for (double snr : snrValues) {
        double frameSuccessRate = 0.0;
        WifiMode mode = htModes[i];
        double ber = errorRateModels[modelName]->CalculateBer(snr, mode);
        double per = 1 - pow((1 - ber), (8 * frameSize));
        frameSuccessRate = 1 - per;
        dataFile << snr << " " << frameSuccessRate << std::endl;
      }
      dataFile.close();

      outFile << "'" << dataFileName << "' with lines title 'MCS" << i << "'";
      if (i < htModes.size() - 1) {
        outFile << ", ";
      }
    }

    outFile << std::endl;
    outFile.close();
  }

  delete errorRateModels["Nist"];
  delete errorRateModels["Yans"];
  delete errorRateModels["Table"];
  return 0;
}