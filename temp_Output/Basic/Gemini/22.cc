#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-error-rate-model.h"
#include "ns3/command-line.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  const int frameSize = 1000;
  std::vector<std::string> dsssModes = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};

  Gnuplot plot("dsss_error_rate.eps");
  plot.SetTitle("Frame Success Rate vs. SNR for DSSS Rates");
  plot.SetLegend("SNR (dB)", "Frame Success Rate");
  plot.SetTerminal("postscript eps color");

  std::vector<double> snrValues;
  for (double snr = 0; snr <= 20; snr += 1) {
    snrValues.push_back(snr);
  }

  std::vector<ErrorRateModel*> errorRateModels;
  errorRateModels.push_back(new YansErrorRateModel());
  errorRateModels.push_back(new NistErrorRateModel());

  TableErrorRateModel *tableModel = new TableErrorRateModel();
  tableModel->SetDefaultRate(0.0);
  errorRateModels.push_back(tableModel);

  std::vector<std::string> modelNames = {"Yans", "Nist", "Table"};

  for (size_t i = 0; i < errorRateModels.size(); ++i) {
    Gnuplot2dDataset dataset;
    dataset.SetTitle(modelNames[i]);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (const std::string& mode : dsssModes) {
      for (double snr : snrValues) {
        double successRate = 1.0;
        if (i == 0) {
            Ptr<YansErrorRateModel> yansModel = DynamicCast<YansErrorRateModel>(GetObject<ErrorRateModel>(errorRateModels[i]));
            double ber = yansModel->GetBitErrorRate(snr, mode);
            successRate = pow(1 - ber, frameSize * 8);
        } else if (i == 1) {
            Ptr<NistErrorRateModel> nistModel = DynamicCast<NistErrorRateModel>(GetObject<ErrorRateModel>(errorRateModels[i]));
            double ber = nistModel->GetBitErrorRate(snr, mode);
            successRate = pow(1 - ber, frameSize * 8);
        } else {
           Ptr<TableErrorRateModel> tableModelPtr = DynamicCast<TableErrorRateModel>(GetObject<ErrorRateModel>(errorRateModels[i]));
           double ber = tableModelPtr->GetBitErrorRate(snr, mode);
           successRate = pow(1 - ber, frameSize * 8);

           tableModelPtr->SetEntry(snr, mode, ber);
        }

        dataset.Add(snr, successRate);

        if (successRate < 0 || successRate > 1) {
           std::cerr << "Error: Success rate out of range [0, 1]: " << successRate << std::endl;
        }
      }
    }
    plot.AddDataset(dataset);
  }

  plot.GenerateOutput();

  for (ErrorRateModel* model : errorRateModels) {
      delete model;
  }

  return 0;
}