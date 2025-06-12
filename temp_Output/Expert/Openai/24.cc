#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <sstream>
#include <iomanip>

using namespace ns3;

struct SimConfig
{
  uint32_t frameSize = 1200;
  uint32_t mcsStart = 0;
  uint32_t mcsStop = 7;
  uint32_t mcsStep = 1;
  double snrStart = 0.0;
  double snrStop = 30.0;
  double snrStep = 1.0;
  bool testNist = true;
  bool testYans = true;
  bool testTable = true;
  std::string outputFile = "fer_vs_snr.eps";
};

double CalcFer(Ptr<ErrorRateModel> errModel, WifiTxVector txVector, uint32_t frameSizeBytes, double snr)
{
  double noiseDbm = 0;
  double rxPowerDbm = snr;
  double snrLinear = std::pow(10.0, snr / 10.0);

  double ber = errModel->GetChunkSuccessRate(txVector, 1, snrLinear, 0, 0);
  if (ber < 0.0)
    return 1.0;
  double per = 1.0 - std::pow(ber, frameSizeBytes * 8);
  return per;
}

int main(int argc, char *argv[])
{
  SimConfig cfg;
  CommandLine cmd;
  cmd.AddValue("frameSize", "Frame size [bytes]", cfg.frameSize);
  cmd.AddValue("mcsStart", "Starting MCS value", cfg.mcsStart);
  cmd.AddValue("mcsStop", "Stopping MCS value", cfg.mcsStop);
  cmd.AddValue("mcsStep", "MCS increment", cfg.mcsStep);
  cmd.AddValue("snrStart", "Starting SNR [dB]", cfg.snrStart);
  cmd.AddValue("snrStop", "Stopping SNR [dB]", cfg.snrStop);
  cmd.AddValue("snrStep", "SNR increment [dB]", cfg.snrStep);
  cmd.AddValue("testNist", "Test Nist error rate model", cfg.testNist);
  cmd.AddValue("testYans", "Test Yans error rate model", cfg.testYans);
  cmd.AddValue("testTable", "Test Table-based error rate model", cfg.testTable);
  cmd.AddValue("outputFile", "Output plot filename (.eps)", cfg.outputFile);
  cmd.Parse(argc, argv);

  std::vector<std::pair<std::string, Ptr<ErrorRateModel>>> errorModels;
  if (cfg.testNist)
    errorModels.emplace_back("Nist", CreateObject<NistErrorRateModel>());
  if (cfg.testYans)
    errorModels.emplace_back("Yans", CreateObject<YansErrorRateModel>());
  if (cfg.testTable)
    errorModels.emplace_back("Table", CreateObject<TableBasedErrorRateModel>());

  std::vector<uint8_t> mcsValues;
  for (uint32_t mcs = cfg.mcsStart; mcs <= cfg.mcsStop; mcs += cfg.mcsStep)
    mcsValues.push_back(static_cast<uint8_t>(mcs));

  std::vector<double> snrValues;
  for (double snr = cfg.snrStart; snr <= cfg.snrStop + 1e-6; snr += cfg.snrStep)
    snrValues.push_back(snr);

  Gnuplot plot(cfg.outputFile);
  plot.SetTitle("FER vs SNR for different Error Rate Models (802.11n, 20MHz)");
  plot.SetTerminal("postscript eps color enhanced");
  plot.SetLegend("SNR (dB)", "Frame Error Rate (FER)");
  plot.AppendExtra("set logscale y");
  plot.AppendExtra("set key outside");
  plot.AppendExtra("set grid");

  std::map<std::string, std::string> colors;
  colors["Nist"] = "red";
  colors["Yans"] = "blue";
  colors["Table"] = "green";

  std::map<std::string, std::map<uint8_t, Gnuplot2dDataset>> datasets;

  for (const auto& model : errorModels)
  {
    for (uint8_t mcs : mcsValues)
    {
      std::ostringstream oss;
      oss << model.first << " (MCS " << unsigned(mcs) << ")";
      Gnuplot2dDataset dataset;
      dataset.SetStyle(Gnuplot2dDataset::LINES);
      dataset.SetTitle(oss.str());
      std::string color = colors[model.first];
      dataset.SetExtra("lt rgb \"" + color + "\" pt 1 lw 2");

      for (double snr : snrValues)
      {
        WifiTxVector txVector;
        txVector.SetMode(WifiMode("HtMcs" + std::to_string(mcs)));
        txVector.SetChannelWidth(20);
        txVector.SetNss(1);
        txVector.SetGuardInterval(WIFI_GI_LONG);
        txVector.SetPreambleType(WIFI_PREAMBLE_HT_MF);

        double fer = CalcFer(model.second, txVector, cfg.frameSize, snr);
        if (fer < 1e-9)
          fer = 1e-9;
        dataset.Add(snr, fer);
      }
      datasets[model.first][mcs] = dataset;
    }
  }

  // Order: For each model, group all MCS 0, 4, 7 curves for better clarity.
  for (const auto& model : errorModels)
  {
    for (uint8_t mcs : mcsValues)
    {
      if (mcs == 0 || mcs == 4 || mcs == 7)
        plot.AddDataset(datasets[model.first][mcs]);
    }
  }

  std::ofstream out(cfg.outputFile);
  plot.GenerateOutput(out);
  out.close();

  return 0;
}